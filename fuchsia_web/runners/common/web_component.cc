// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/common/web_component.h"

#include <fuchsia/logger/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/fit/function.h>
#include <lib/sys/cpp/component_context.h>

#include <string_view>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "fuchsia_web/runners/common/web_content_runner.h"

WebComponent::WebComponent(std::string_view debug_name,
                           WebContentRunner* runner,
                           std::unique_ptr<base::StartupContext> context)
    : debug_name_(debug_name),
      runner_(runner),
      startup_context_(std::move(context)),
      navigation_listener_binding_(this) {
  DCHECK(!debug_name_.empty());
  DCHECK(runner);

  LOG(INFO) << "Creating component " << debug_name_;
}

WebComponent::~WebComponent() = default;

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
    DestroyComponent(status);
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

void WebComponent::CreateViewWithViewRef(
    zx::eventpair view_token_value,
    fuchsia::ui::views::ViewRefControl control_ref,
    fuchsia::ui::views::ViewRef view_ref) {
  DCHECK(frame_);
  if (view_is_bound_) {
    LOG(ERROR) << "CreateView() called more than once.";
    DestroyComponent(ZX_ERR_BAD_STATE);
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
    DestroyComponent(ZX_ERR_BAD_STATE);
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
        DestroyComponent(ZX_ERR_INTERNAL);
        break;
      case fuchsia::web::PageType::NORMAL:
        break;
    }
  }
  // Do not touch |this|, which may have been deleted by DestroyComponent().

  // |callback| is safe to run, since it is on the stack.
  callback();
}

void WebComponent::DestroyComponent(int64_t exit_code) {
  LOG(INFO) << "Component " << debug_name_ << " is shutting down."
            << " exit_code=" << exit_code;

  runner_->DestroyComponent(this);
  // `this` is no longer valid.
}

void WebComponent::CloseFrameWithTimeout(base::TimeDelta timeout) {
  if (frame_) {
    runner_->CloseFrameWithTimeout(std::move(frame_), timeout);
  }
}
