// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/pending_cast_component.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/strings/string_piece.h"

PendingCastComponent::PendingCastComponent(
    Delegate* delegate,
    std::unique_ptr<base::StartupContext> startup_context,
    fidl::InterfaceRequest<fuchsia::component::runner::ComponentController>
        controller_request,
    base::StringPiece app_id)
    : delegate_(delegate), app_id_(app_id) {
  DCHECK(startup_context);
  DCHECK(controller_request);

  // Store the supplied CastComponent parameters in |params_|.
  params_.startup_context = std::move(startup_context);
  params_.controller_request = std::move(controller_request);

  // Request the application's configuration, including the identity of the
  // Agent that should provide component-specific resources, e.g. API bindings.
  // TODO(https://crbug.com/1065707): Access the ApplicationConfigManager via
  // the Runner's incoming service directory once it is available there.
  params_.startup_context->svc()->Connect(
      application_config_manager_.NewRequest());
  application_config_manager_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "ApplicationConfigManager disconnected.";
    delegate_->CancelPendingComponent(this);
  });
  application_config_manager_->GetConfig(
      std::string(app_id),
      fit::bind_member(this,
                       &PendingCastComponent::OnApplicationConfigReceived));
}

PendingCastComponent::~PendingCastComponent() = default;

void PendingCastComponent::OnApplicationConfigReceived(
    chromium::cast::ApplicationConfig application_config) {
  if (application_config.IsEmpty()) {
    DLOG(WARNING) << "No application config was found.";
    delegate_->CancelPendingComponent(this);
    return;
  }

  if (!application_config.has_web_url()) {
    DLOG(WARNING) << "Only web-based applications are supported.";
    delegate_->CancelPendingComponent(this);
    return;
  }

  params_.application_config = std::move(application_config);

  // Request custom API bindings from the component's Agent.
  params_.api_bindings_client = std::make_unique<ApiBindingsClient>(
      params_.startup_context->svc()->Connect<chromium::cast::ApiBindings>(),
      base::BindOnce(&PendingCastComponent::OnApiBindingsInitialized,
                     base::Unretained(this)));

  // Request UrlRequestRewriteRulesProvider from the Agent.
  params_.startup_context->svc()->Connect(
      params_.url_rewrite_rules_provider.NewRequest());
  params_.url_rewrite_rules_provider.set_error_handler([this](
                                                           zx_status_t status) {
    if (status != ZX_ERR_PEER_CLOSED) {
      ZX_LOG(ERROR, status) << "UrlRequestRewriteRulesProvider disconnected.";
      delegate_->CancelPendingComponent(this);
      return;
    }
    ZX_DLOG(WARNING, status) << "UrlRequestRewriteRulesProvider unsupported.";
    params_.initial_url_rewrite_rules =
        std::vector<fuchsia::web::UrlRequestRewriteRule>();
    MaybeLaunchComponent();
  });
  params_.url_rewrite_rules_provider->GetUrlRequestRewriteRules(
      [this](std::vector<fuchsia::web::UrlRequestRewriteRule> rewrite_rules) {
        params_.initial_url_rewrite_rules.emplace(std::move(rewrite_rules));
        MaybeLaunchComponent();
      });

  // Connect to the component-specific ApplicationContext to retrieve the
  // media-session identifier assigned to this instance.
  application_context_ = params_.startup_context->svc()
                             ->Connect<chromium::cast::ApplicationContext>();
  application_context_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "ApplicationContext disconnected.";
    delegate_->CancelPendingComponent(this);
  });

  if (params_.application_config.has_audio_renderer_usage()) {
    DCHECK(!params_.media_settings);
    params_.media_settings = fuchsia::web::FrameMediaSettings{};
    params_.media_settings->set_renderer_usage(
        params_.application_config.audio_renderer_usage());
  } else {
    // If `audio_renderer_usage` is not specified then `AudioConsumer` is used
    // for that app. We need to fetch `session_id` in that case.
    application_context_->GetMediaSessionId([this](uint64_t session_id) {
      DCHECK(!params_.media_settings);
      params_.media_settings = fuchsia::web::FrameMediaSettings{};
      if (session_id > 0)
        params_.media_settings->set_audio_consumer_session_id(session_id);

      MaybeLaunchComponent();
    });
  }
}

void PendingCastComponent::OnApiBindingsInitialized() {
  if (params_.api_bindings_client->HasBindings())
    MaybeLaunchComponent();
  else
    delegate_->CancelPendingComponent(this);
}

void PendingCastComponent::MaybeLaunchComponent() {
  if (!params_.AreComplete())
    return;

  // Clear the error handlers on InterfacePtr<>s before passing them, to avoid
  // user-after-free of |this|.
  params_.url_rewrite_rules_provider.set_error_handler(nullptr);

  params_.application_context = application_context_.Unbind();

  delegate_->LaunchPendingComponent(this, std::move(params_));
}
