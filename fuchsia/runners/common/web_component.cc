// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/common/web_component.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/fit/function.h>
#include <lib/sys/cpp/component_context.h>
#include <utility>

#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "fuchsia/runners/common/web_content_runner.h"

WebComponent::WebComponent(
    WebContentRunner* runner,
    std::unique_ptr<base::fuchsia::StartupContext> context,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request)
    : runner_(runner),
      startup_context_(std::move(context)),
      controller_binding_(this),
      module_context_(
          startup_context()
              ->incoming_services()
              ->ConnectToService<fuchsia::modular::ModuleContext>()) {
  DCHECK(runner);

  // If the ComponentController request is valid then bind it, and configure it
  // to destroy this component on error.
  if (controller_request.is_valid()) {
    controller_binding_.Bind(std::move(controller_request));
    controller_binding_.set_error_handler([this](zx_status_t status) {
      ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
          << " ComponentController disconnected";
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

  // Send process termination details to the client.
  controller_binding_.events().OnTerminated(termination_exit_code_,
                                            termination_reason_);
}

void WebComponent::EnableRemoteDebugging() {
  DCHECK(!component_started_);
  enable_remote_debugging_ = true;
}

void WebComponent::StartComponent() {
  DCHECK(!component_started_);

  // Create the underlying Frame and get its NavigationController.
  fuchsia::web::CreateFrameParams create_params;
  create_params.set_enable_remote_debugging(enable_remote_debugging_);
  runner_->GetContext()->CreateFrameWithParams(std::move(create_params),
                                               frame_.NewRequest());

  // If the Frame unexpectedly disconnect us then tear-down this Component.
  frame_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " Frame disconnected";
    DestroyComponent(0, fuchsia::sys::TerminationReason::EXITED);
  });

  if (startup_context()->has_outgoing_directory_request()) {
    // Publish outgoing services and start serving component's outgoing
    // directory.
    view_provider_binding_ = std::make_unique<
        base::fuchsia::ScopedServiceBinding<fuchsia::ui::app::ViewProvider>>(
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
  // Signal abnormal process termination.
  DestroyComponent(1, fuchsia::sys::TerminationReason::RUNNER_TERMINATED);
}

void WebComponent::Detach() {
  controller_binding_.set_error_handler(nullptr);
}

void WebComponent::CreateView(
    zx::eventpair view_token_value,
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
    fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services) {
  DCHECK(frame_);
  DCHECK(!view_is_bound_);

  fuchsia::ui::views::ViewToken view_token;
  view_token.value = std::move(view_token_value);
  frame_->CreateView(std::move(view_token));

  view_is_bound_ = true;
}

void WebComponent::DestroyComponent(int termination_exit_code,
                                    fuchsia::sys::TerminationReason reason) {
  termination_reason_ = reason;
  termination_exit_code_ = termination_exit_code;
  runner_->DestroyComponent(this);
}
