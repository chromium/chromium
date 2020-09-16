// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_component.h"

#include <lib/fidl/cpp/binding.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/path_service.h"
#include "base/task/current_thread.h"
#include "fuchsia/base/agent_manager.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "fuchsia/runners/cast/cast_streaming.h"
#include "fuchsia/runners/common/web_component.h"

namespace {

constexpr int kBindingsFailureExitCode = 129;
constexpr int kRewriteRulesProviderDisconnectExitCode = 130;

}  // namespace

CastComponent::Params::Params() = default;
CastComponent::Params::Params(Params&&) = default;
CastComponent::Params::~Params() = default;

bool CastComponent::Params::AreComplete() const {
  if (application_config.IsEmpty())
    return false;
  if (!api_bindings_client->HasBindings())
    return false;
  if (!initial_url_rewrite_rules)
    return false;
  if (!media_session_id)
    return false;
  return true;
}

CastComponent::CastComponent(WebContentRunner* runner,
                             CastComponent::Params params,
                             bool is_headless)
    : WebComponent(runner,
                   std::move(params.startup_context),
                   std::move(params.controller_request)),
      is_headless_(is_headless),
      agent_manager_(std::move(params.agent_manager)),
      application_config_(std::move(params.application_config)),
      url_rewrite_rules_provider_(std::move(params.url_rewrite_rules_provider)),
      initial_url_rewrite_rules_(
          std::move(params.initial_url_rewrite_rules.value())),
      api_bindings_client_(std::move(params.api_bindings_client)),
      application_context_(params.application_context.Bind()),
      media_session_id_(params.media_session_id.value()),
      headless_disconnect_watch_(FROM_HERE) {
  base::AutoReset<bool> constructor_active_reset(&constructor_active_, true);
}

CastComponent::~CastComponent() = default;

void CastComponent::SetOnDestroyedCallback(base::OnceClosure on_destroyed) {
  on_destroyed_ = std::move(on_destroyed);
}

void CastComponent::StartComponent() {
  if (application_config_.has_enable_remote_debugging() &&
      application_config_.enable_remote_debugging()) {
    WebComponent::EnableRemoteDebugging();
  }

  WebComponent::StartComponent();

  connector_ = std::make_unique<NamedMessagePortConnector>(frame());

  url_rewrite_rules_provider_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_OK, status)
        << "UrlRequestRewriteRulesProvider disconnected.";
    DestroyComponent(kRewriteRulesProviderDisconnectExitCode,
                     fuchsia::sys::TerminationReason::INTERNAL_ERROR);
  });
  OnRewriteRulesReceived(std::move(initial_url_rewrite_rules_));

  frame()->SetMediaSessionId(media_session_id_);
  frame()->ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                               fuchsia::web::AllowInputState::DENY);
  frame()->SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel::WARN);

  if (IsAppConfigForCastStreaming(application_config_)) {
    // TODO(crbug.com/1082821): Remove this once the Cast Streaming Receiver
    // component has been implemented.

    // Register the MessagePort for the Cast Streaming Receiver.
    fidl::InterfaceHandle<fuchsia::web::MessagePort> message_port;
    fuchsia::web::WebMessage message;
    message.set_data(cr_fuchsia::MemBufferFromString("", "empty_message"));
    fuchsia::web::OutgoingTransferable outgoing_transferable;
    outgoing_transferable.set_message_port(message_port.NewRequest());
    std::vector<fuchsia::web::OutgoingTransferable> outgoing_transferables;
    outgoing_transferables.push_back(std::move(outgoing_transferable));
    message.set_outgoing_transfer(std::move(outgoing_transferables));

    frame()->PostMessage(
        kCastStreamingMessagePortOrigin, std::move(message),
        [this](fuchsia::web::Frame_PostMessage_Result result) {
          if (result.is_err()) {
            DestroyComponent(kBindingsFailureExitCode,
                             fuchsia::sys::TerminationReason::INTERNAL_ERROR);
          }
        });
    api_bindings_client_->OnPortConnected(kCastStreamingMessagePortName,
                                          std::move(message_port));
  }

  api_bindings_client_->AttachToFrame(
      frame(), connector_.get(),
      base::BindOnce(&CastComponent::DestroyComponent, base::Unretained(this),
                     kBindingsFailureExitCode,
                     fuchsia::sys::TerminationReason::INTERNAL_ERROR));

  // Media loading has to be unblocked by the agent via the
  // ApplicationController.
  frame()->SetBlockMediaLoading(true);

  if (application_config_.has_force_content_dimensions()) {
    frame()->ForceContentDimensions(std::make_unique<fuchsia::ui::gfx::vec2>(
        application_config_.force_content_dimensions()));
  }

  application_controller_ = std::make_unique<ApplicationControllerImpl>(
      frame(), application_context_.get());

  // Pass application permissions to the frame.
  if (application_config_.has_permissions()) {
    std::string origin = GURL(application_config_.web_url()).GetOrigin().spec();
    for (auto& permission : application_config_.permissions()) {
      fuchsia::web::PermissionDescriptor permission_clone;
      zx_status_t status = permission.Clone(&permission_clone);
      ZX_DCHECK(status == ZX_OK, status);
      frame()->SetPermissionState(std::move(permission_clone), origin,
                                  fuchsia::web::PermissionState::GRANTED);
    }
  }
}

void CastComponent::DestroyComponent(int64_t exit_code,
                                     fuchsia::sys::TerminationReason reason) {
  DCHECK(!constructor_active_);

  std::move(on_destroyed_).Run();

  // If the component EXITED then pass the |exit_code| to the Agent, to allow it
  // to distinguish graceful termination from crashes.
  if (reason == fuchsia::sys::TerminationReason::EXITED &&
      application_controller_) {
    application_context_->OnApplicationExit(exit_code);
  }

  // frame() is about to be destroyed, so there is no need to perform cleanup
  // such as removing before-load JavaScripts.
  api_bindings_client_->DetachFromFrame(frame());

  WebComponent::DestroyComponent(exit_code, reason);
}

void CastComponent::OnRewriteRulesReceived(
    std::vector<fuchsia::web::UrlRequestRewriteRule> rewrite_rules) {
  frame()->SetUrlRequestRewriteRules(std::move(rewrite_rules), [this]() {
    url_rewrite_rules_provider_->GetUrlRequestRewriteRules(
        fit::bind_member(this, &CastComponent::OnRewriteRulesReceived));
  });
}

void CastComponent::OnNavigationStateChanged(
    fuchsia::web::NavigationState change,
    OnNavigationStateChangedCallback callback) {
  if (change.has_is_main_document_loaded() && change.is_main_document_loaded())
    connector_->OnPageLoad();
  WebComponent::OnNavigationStateChanged(std::move(change),
                                         std::move(callback));
}

void CastComponent::CreateView(
    zx::eventpair view_token,
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
    fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services) {
  scenic::ViewRefPair view_ref_pair = scenic::ViewRefPair::New();
  CreateViewWithViewRef(std::move(view_token),
                        std::move(view_ref_pair.control_ref),
                        std::move(view_ref_pair.view_ref));
}

void CastComponent::CreateViewWithViewRef(
    zx::eventpair view_token,
    fuchsia::ui::views::ViewRefControl control_ref,
    fuchsia::ui::views::ViewRef view_ref) {
  if (is_headless_) {
    // For headless CastComponents, |view_token| does not actually connect to a
    // Scenic View. It is merely used as a conduit for propagating termination
    // signals.
    headless_view_token_ = std::move(view_token);
    base::CurrentIOThread::Get()->WatchZxHandle(
        headless_view_token_.get(), false /* persistent */,
        ZX_SOCKET_PEER_CLOSED, &headless_disconnect_watch_, this);

    frame()->EnableHeadlessRendering();
    return;
  }

  WebComponent::CreateViewWithViewRef(
      std::move(view_token), std::move(control_ref), std::move(view_ref));
}

void CastComponent::OnZxHandleSignalled(zx_handle_t handle,
                                        zx_signals_t signals) {
  DCHECK_EQ(signals, ZX_SOCKET_PEER_CLOSED);
  DCHECK(is_headless_);

  frame()->DisableHeadlessRendering();
}
