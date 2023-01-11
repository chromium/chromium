// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/common/web_component.h"

#include <fuchsia/logger/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/fit/function.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "fuchsia_web/runners/common/web_content_runner.h"

WebComponent::WebComponent(
    base::StringPiece debug_name,
    WebContentRunner* runner,
    std::unique_ptr<base::StartupContext> context,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request)
    : debug_name_(debug_name),
      runner_(runner),
      startup_context_(std::move(context)),
      controller_binding_(this),
      module_context_(
          startup_context()->svc()->Connect<fuchsia::modular::ModuleContext>()),
      navigation_listener_binding_(this) {
  DCHECK(!debug_name_.empty());
  DCHECK(runner);

  LOG(INFO) << "Creating component " << debug_name_;

  // If the ComponentController request is valid then bind it, and configure it
  // to destroy this component on error.
  if (controller_request.is_valid()) {
    controller_binding_.Bind(std::move(controller_request));
    controller_binding_.set_error_handler([this](zx_status_t status) {
      ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
          << " ComponentController disconnected for component " << debug_name_;
      // Teardown the component with dummy values, since ComponentController
      // channel isn't there to receive them.
      DestroyComponent(0, fuchsia::sys::TerminationReason::UNKNOWN);
    });
  }
}

WebComponent::~WebComponent() {
  // If Modular is available, request to be removed from the Story.
  if (module_context_)
    module_context_->RemoveSelfFromStory();

  if (controller_binding_.is_bound()) {
    // Send process termination details to the client.
    controller_binding_.events().OnTerminated(termination_exit_code_,
                                              termination_reason_);
  }
}

void WebComponent::EnableRemoteDebugging() {
  DCHECK(!component_started_);
  enable_remote_debugging_ = true;
}

void WebComponent::StartComponent() {
  DCHECK(!component_started_);

  // Create the underlying Frame and get its NavigationController.
  fuchsia::web::CreateFrameParams create_params;
  if (!debug_name_.empty())
    create_params.set_debug_name(debug_name_);
  create_params.set_enable_remote_debugging(enable_remote_debugging_);
  runner_->CreateFrameWithParams(std::move(create_params), frame_.NewRequest());

  // If the Frame unexpectedly disconnects then tear-down this Component.
  // ZX_OK indicates intentional termination (e.g. via window.close()).
  // ZX_ERR_PEER_CLOSED will usually indicate a crash, reported elsewhere.
  // Therefore only log other, more unusual, |status| codes.
  frame_.set_error_handler([this](zx_status_t status) {
    if (status != ZX_OK && status != ZX_ERR_PEER_CLOSED) {
      ZX_LOG(ERROR, status)
          << " component " << debug_name_ << ": Frame disconnected";
    }
    DestroyComponent(status, fuchsia::sys::TerminationReason::EXITED);
  });

  // Route logging from the Frame to the component's LogSink.
  frame_->SetConsoleLogSink(
      startup_context()->svc()->Connect<fuchsia::logger::LogSink>());

  fuchsia::web::ContentAreaSettings settings;
  settings.set_autoplay_policy(
      fuchsia::web::AutoplayPolicy::REQUIRE_USER_ACTIVATION);
  frame_->SetContentAreaSettings(std::move(settings));

  // Observe the Frame for failures, via navigation state change events.
  frame_->SetNavigationEventListener2(navigation_listener_binding_.NewBinding(),
                                      /*flags=*/{});

  if (startup_context()->has_outgoing_directory_request()) {
    // Publish outgoing services and start serving component's outgoing
    // directory.
    view_provider_binding_ = std::make_unique<
        base::ScopedServiceBinding<fuchsia::ui::app::ViewProvider>>(
        startup_context()->component_context()->outgoing().get(), this);
    lifecycle_ = std::make_unique<cr_fuchsia::LifecycleImpl>(
        startup_context()->component_context()->outgoing().get(),
        base::BindOnce(&WebComponent::Kill, base::Unretained(this)));
    startup_context()->ServeOutgoingDirectory();
  }

  component_started_ = true;
}

void WebComponent::LoadUrl(
    const GURL& url,
    std::vector<fuchsia::net::http::Header> extra_headers) {
  DCHECK(url.is_valid());

  fuchsia::web::NavigationControllerPtr navigation_controller;
  frame()->GetNavigationController(navigation_controller.NewRequest());

  // Set the page activation flag on the initial load, so that features like
  // autoplay work as expected when a WebComponent first loads the specified
  // content.
  fuchsia::web::LoadUrlParams params;
  params.set_was_user_activated(true);
  if (!extra_headers.empty())
    params.set_headers(std::move(extra_headers));

  navigation_controller->LoadUrl(
      url.spec(), std::move(params),
      [](fuchsia::web::NavigationController_LoadUrl_Result) {});
}

void WebComponent::Kill() {
  // Close the web content, allowing the `frame_` implementation to use its
  // default timeout.
  frame_->Close({});
}

void WebComponent::Detach() {
  controller_binding_.set_error_handler(nullptr);
}

void WebComponent::CreateView(
    zx::eventpair view_token_value,
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
    fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services) {
  scenic::ViewRefPair view_ref_pair = scenic::ViewRefPair::New();
  CreateViewWithViewRef(std::move(view_token_value),
                        std::move(view_ref_pair.control_ref),
                        std::move(view_ref_pair.view_ref));
}

void WebComponent::CreateViewWithViewRef(
    zx::eventpair view_token_value,
    fuchsia::ui::views::ViewRefControl control_ref,
    fuchsia::ui::views::ViewRef view_ref) {
  DCHECK(frame_);
  if (view_is_bound_) {
    LOG(ERROR) << "CreateView() called more than once.";
    DestroyComponent(ZX_ERR_BAD_STATE, fuchsia::sys::TerminationReason::EXITED);
    return;
  }

  fuchsia::ui::views::ViewToken view_token;
  view_token.value = std::move(view_token_value);
  frame_->CreateViewWithViewRef(std::move(view_token), std::move(control_ref),
                                std::move(view_ref));

  view_is_bound_ = true;
}

void WebComponent::CreateView2(fuchsia::ui::app::CreateView2Args view_args) {
  DCHECK(frame_);
  if (view_is_bound_) {
    LOG(ERROR) << "CreateView() called more than once.";
    DestroyComponent(ZX_ERR_BAD_STATE, fuchsia::sys::TerminationReason::EXITED);
    return;
  }

  fuchsia::web::CreateView2Args web_view_args;
  web_view_args.set_view_creation_token(
      std::move(*view_args.mutable_view_creation_token()));
  frame_->CreateView2(std::move(web_view_args));

  view_is_bound_ = true;
}

void WebComponent::OnNavigationStateChanged(
    fuchsia::web::NavigationState change,
    OnNavigationStateChangedCallback callback) {
  if (change.has_page_type()) {
    switch (change.page_type()) {
      case fuchsia::web::PageType::ERROR:
        DestroyComponent(ZX_ERR_INTERNAL,
                         fuchsia::sys::TerminationReason::EXITED);
        break;
      case fuchsia::web::PageType::NORMAL:
        break;
    }
  }
  // Do not touch |this|, which may have been deleted by DestroyComponent().

  // |callback| is safe to run, since it is on the stack.
  callback();
}

void WebComponent::DestroyComponent(int64_t exit_code,
                                    fuchsia::sys::TerminationReason reason) {
  LOG(INFO) << "Component " << debug_name_
            << " is shutting down. reason=" << static_cast<int>(reason)
            << " exit_code=" << exit_code;

  termination_reason_ = reason;
  termination_exit_code_ = exit_code;

  runner_->DestroyComponent(this);
  // `this` is no longer valid.
}

void WebComponent::CloseFrameWithTimeout(base::TimeDelta timeout) {
  if (frame_) {
    runner_->CloseFrameWithTimeout(std::move(frame_), timeout);
  }
}
