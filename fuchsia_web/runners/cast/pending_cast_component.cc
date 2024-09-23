// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/pending_cast_component.h"

#include <fidl/fuchsia.io/cpp/hlcpp_conversion.h>
#include <lib/async/default.h>
#include <lib/trace/event.h>

#include <string_view>

#include "base/check.h"
#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"

PendingCastComponent::PendingCastComponent(
    Delegate* delegate,
    std::unique_ptr<base::StartupContext> startup_context,
    fidl::InterfaceRequest<fuchsia::component::runner::ComponentController>
        controller_request,
    std::string_view app_id)
    : delegate_(delegate),
      app_id_(app_id),
      application_context_error_handler_(base::BindRepeating(
          &PendingCastComponent::OnApplicationContextFidlError,
          base::Unretained(this))) {
  params_.trace_flow_id = TRACE_NONCE();
  TRACE_DURATION("cast_runner", "Create PendingCastComponent", "app_id",
                 app_id_.c_str());
  TRACE_FLOW_BEGIN("cast_runner", "CastComponent", params_.trace_flow_id,
                   "app_id", app_id_.c_str());

  DCHECK(startup_context);
  DCHECK(controller_request);

  // Store the supplied CastComponent parameters in |params_|.
  params_.startup_context = std::move(startup_context);
  params_.controller_request = std::move(controller_request);

  // Request the application's configuration, including the identity of the
  // Agent that should provide component-specific resources, e.g. API bindings.
  base::ComponentContextForProcess()->svc()->Connect(
      application_config_manager_.NewRequest());
  application_config_manager_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "ApplicationConfigManager disconnected.";
    CancelComponent();
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
    CancelComponent();
    return;
  }

  if (!application_config.has_web_url()) {
    DLOG(WARNING) << "Only web-based applications are supported.";
    CancelComponent();
    return;
  }

  params_.application_config = std::move(application_config);
  fidl::ClientEnd<fuchsia_io::Directory> startup_context_svc_dir =
      fidl::HLCPPToNatural(params_.startup_context->svc()->CloneChannel());

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
      CancelComponent();
      return;
    }

    TRACE_DURATION("cast_runner", "GetUrlRequestRewriteRules error");
    TRACE_FLOW_STEP("cast_runner", "CastComponent", params_.trace_flow_id);

    ZX_DLOG(WARNING, status) << "UrlRequestRewriteRulesProvider unsupported.";
    params_.initial_url_rewrite_rules =
        std::vector<fuchsia::web::UrlRequestRewriteRule>();
    MaybeLaunchComponent();
  });
  params_.url_rewrite_rules_provider->GetUrlRequestRewriteRules(
      [this](std::vector<fuchsia::web::UrlRequestRewriteRule> rewrite_rules) {
        {
          TRACE_DURATION("cast_runner", "GetUrlRequestRewriteRules result");
          TRACE_FLOW_STEP("cast_runner", "CastComponent",
                          params_.trace_flow_id);
          params_.initial_url_rewrite_rules.emplace(std::move(rewrite_rules));
        }
        MaybeLaunchComponent();
      });

  auto application_context_client_end =
      base::fuchsia_component::ConnectAt<chromium_cast::ApplicationContext>(
          startup_context_svc_dir.borrow());
  if (application_context_client_end.is_error()) {
    LOG(ERROR) << base::FidlConnectionErrorMessage(
        application_context_client_end);
    return;
  }
  // Connect to the component-specific ApplicationContext to retrieve the
  // media-session identifier assigned to this instance.
  application_context_.Bind(std::move(application_context_client_end.value()),
                            async_get_default_dispatcher(),
                            &application_context_error_handler_);

  if (params_.application_config.has_audio_renderer_usage()) {
    DCHECK(!params_.media_settings);
    params_.media_settings = fuchsia::web::FrameMediaSettings{};
    params_.media_settings->set_renderer_usage(
        params_.application_config.audio_renderer_usage());
  } else {
    // If `audio_renderer_usage` is not specified then `AudioConsumer` is used
    // for that app. We need to fetch `session_id` in that case.
    application_context_->GetMediaSessionId().Then(
        [this](
            fidl::Result<chromium_cast::ApplicationContext::GetMediaSessionId>&
                result) {
          {
            TRACE_DURATION("cast_runner", "GetMediaSessionId result");
            TRACE_FLOW_STEP("cast_runner", "CastComponent",
                            params_.trace_flow_id);

            DCHECK(!params_.media_settings);
            if (result.is_error()) {
              LOG(ERROR) << base::FidlMethodResultErrorMessage(
                  result, "GetMediaSessionId");
              delegate_->CancelPendingComponent(this);
              return;
            }
            params_.media_settings = fuchsia::web::FrameMediaSettings{};
            if (result->media_session_id() > 0) {
              params_.media_settings->set_audio_consumer_session_id(
                  result->media_session_id());
            }
          }

          MaybeLaunchComponent();
        });
  }
}

void PendingCastComponent::OnApiBindingsInitialized() {
  {
    TRACE_DURATION("cast_runner", "OnApiBindingsInitialized");
    TRACE_FLOW_STEP("cast_runner", "CastComponent", params_.trace_flow_id);
  }

  if (params_.api_bindings_client->HasBindings()) {
    MaybeLaunchComponent();
  } else {
    CancelComponent();
  }
}

void PendingCastComponent::MaybeLaunchComponent() {
  if (!params_.AreComplete()) {
    return;
  }

  // Clear the error handlers on InterfacePtr<>s before passing them, to avoid
  // user-after-free of |this|.
  params_.url_rewrite_rules_provider.set_error_handler(nullptr);

  auto result = application_context_.UnbindMaybeGetEndpoint();
  if (result.is_error()) {
    ZX_LOG(ERROR, result.error_value().status());
    return;
  }
  params_.application_context = std::move(result.value());

  delegate_->LaunchPendingComponent(this, std::move(params_));
}

void PendingCastComponent::OnApplicationContextFidlError(
    fidl::UnbindInfo error) {
  ZX_LOG(ERROR, error.status()) << "ApplicationContext disconnected.";
  CancelComponent();
}

void PendingCastComponent::CancelComponent() {
  TRACE_DURATION("cast_runner", "PendingCastComponent::CancelComponent");
  TRACE_FLOW_END("cast_runner", "CastComponent", params_.trace_flow_id);

  delegate_->CancelPendingComponent(this);
}
