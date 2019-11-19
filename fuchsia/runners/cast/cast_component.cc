// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_component.h"

#include <lib/fidl/cpp/binding.h>
#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/files/file_util.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/path_service.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "fuchsia/runners/common/web_component.h"

namespace {

constexpr int kBindingsFailureExitCode = 129;
constexpr int kRewriteRulesProviderDisconnectExitCode = 130;

}  // namespace

CastComponent::CastComponentParams::CastComponentParams() = default;
CastComponent::CastComponentParams::CastComponentParams(CastComponentParams&&) =
    default;
CastComponent::CastComponentParams::~CastComponentParams() = default;

CastComponent::CastComponent(CastRunner* runner,
                             CastComponent::CastComponentParams params)
    : WebComponent(runner,
                   std::move(params.startup_context),
                   std::move(params.controller_request)),
      agent_manager_(std::move(params.agent_manager)),
      application_config_(std::move(params.app_config)),
      rewrite_rules_provider_(std::move(params.rewrite_rules_provider)),
      initial_rewrite_rules_(std::move(params.rewrite_rules.value())),
      api_bindings_client_(std::move(params.api_bindings_client)),
      navigation_listener_binding_(this) {
  base::AutoReset<bool> constructor_active_reset(&constructor_active_, true);
}

CastComponent::~CastComponent() = default;

void CastComponent::StartComponent() {
  if (application_config_.has_enable_remote_debugging() &&
      application_config_.enable_remote_debugging()) {
    WebComponent::EnableRemoteDebugging();
  }

  WebComponent::StartComponent();

  connector_ = std::make_unique<NamedMessagePortConnector>(frame());

  rewrite_rules_provider_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_OK, status)
        << "UrlRequestRewriteRulesProvider disconnected.";
    DestroyComponent(kRewriteRulesProviderDisconnectExitCode,
                     fuchsia::sys::TerminationReason::INTERNAL_ERROR);
  });
  OnRewriteRulesReceived(std::move(initial_rewrite_rules_));

  frame()->SetEnableInput(false);
  frame()->SetNavigationEventListener(
      navigation_listener_binding_.NewBinding());
  api_bindings_client_->AttachToFrame(
      frame(), connector_.get(),
      base::BindOnce(&CastComponent::DestroyComponent, base::Unretained(this),
                     kBindingsFailureExitCode,
                     fuchsia::sys::TerminationReason::INTERNAL_ERROR));

  application_controller_ = std::make_unique<ApplicationControllerImpl>(
      frame(), agent_manager_->ConnectToAgentService<
                   chromium::cast::ApplicationControllerReceiver>(
                   CastRunner::kAgentComponentUrl));
}

void CastComponent::DestroyComponent(int termination_exit_code,
                                     fuchsia::sys::TerminationReason reason) {
  DCHECK(!constructor_active_);

  WebComponent::DestroyComponent(termination_exit_code, reason);
}

void CastComponent::OnRewriteRulesReceived(
    std::vector<fuchsia::web::UrlRequestRewriteRule> rewrite_rules) {
  frame()->SetUrlRequestRewriteRules(std::move(rewrite_rules), [this]() {
    rewrite_rules_provider_->GetUrlRequestRewriteRules(
        fit::bind_member(this, &CastComponent::OnRewriteRulesReceived));
  });
}

void CastComponent::OnNavigationStateChanged(
    fuchsia::web::NavigationState change,
    OnNavigationStateChangedCallback callback) {
  if (change.has_is_main_document_loaded() && change.is_main_document_loaded())
    connector_->OnPageLoad();
  callback();
}
