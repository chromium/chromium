// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <memory>
#include <string>
#include <utility>

#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "url/gurl.h"

namespace {

bool AreCastComponentParamsValid(
    const CastComponent::CastComponentParams& params) {
  if (params.app_config.IsEmpty())
    return false;
  if (!params.api_bindings_client->HasBindings())
    return false;
  if (!params.rewrite_rules.has_value())
    return false;
  return true;
}

// Creates a CreateContextParams object which can be used as a basis
// for starting isolated Runners.
fuchsia::web::CreateContextParams BuildCreateContextParamsForIsolatedRunners(
    const fuchsia::web::CreateContextParams& create_context_params) {
  fuchsia::web::CreateContextParams output;

  // Isolated contexts receive only a limited set of features.
  output.set_features(
      fuchsia::web::ContextFeatureFlags::AUDIO |
      fuchsia::web::ContextFeatureFlags::VULKAN |
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER);

  if (create_context_params.has_user_agent_product()) {
    output.set_user_agent_product(create_context_params.user_agent_product());
  }
  if (create_context_params.has_user_agent_version()) {
    output.set_user_agent_version(create_context_params.user_agent_version());
  }
  if (create_context_params.has_remote_debugging_port()) {
    output.set_remote_debugging_port(
        create_context_params.remote_debugging_port());
  }
  return output;
}

}  // namespace

CastRunner::CastRunner(sys::OutgoingDirectory* outgoing_directory,
                       fuchsia::web::CreateContextParams create_context_params)
    : WebContentRunner(outgoing_directory,
                       base::BindOnce(&CastRunner::CreateCastRunnerWebContext,
                                      base::Unretained(this))),
      create_context_params_(std::move(create_context_params)),
      common_create_context_params_(
          BuildCreateContextParamsForIsolatedRunners(create_context_params_)) {}

CastRunner::CastRunner(OnDestructionCallback on_destruction_callback,
                       fuchsia::web::ContextPtr context)
    : WebContentRunner(std::move(context)),
      on_destruction_callback_(std::move(on_destruction_callback)) {}

CastRunner::~CastRunner() = default;

fuchsia::web::ContextPtr CastRunner::CreateCastRunnerWebContext() {
  return WebContentRunner::CreateWebContext(std::move(create_context_params_));
}

void CastRunner::StartComponent(
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  // Verify that |package| specifies a Cast URI, and pull the app-Id from it.
  constexpr char kCastPresentationUrlScheme[] = "cast";
  constexpr char kCastSecurePresentationUrlScheme[] = "casts";

  GURL cast_url(package.resolved_url);
  if (!cast_url.is_valid() ||
      (!cast_url.SchemeIs(kCastPresentationUrlScheme) &&
       !cast_url.SchemeIs(kCastSecurePresentationUrlScheme)) ||
      cast_url.GetContent().empty()) {
    LOG(ERROR) << "Rejected invalid URL: " << cast_url;
    return;
  }

  // The application configuration is obtained asynchronously via the
  // per-component ApplicationConfigManager. The pointer to that service must be
  // kept live until the request completes or CastRunner is deleted.
  auto pending_component =
      std::make_unique<CastComponent::CastComponentParams>();
  pending_component->startup_context =
      std::make_unique<base::fuchsia::StartupContext>(std::move(startup_info));
  pending_component->agent_manager = std::make_unique<cr_fuchsia::AgentManager>(
      pending_component->startup_context->component_context()->svc().get());
  pending_component->controller_request = std::move(controller_request);

  // Get binding details from the Agent.
  fidl::InterfaceHandle<chromium::cast::ApiBindings> api_bindings_client;
  pending_component->agent_manager->ConnectToAgentService(
      kAgentComponentUrl, api_bindings_client.NewRequest());
  pending_component->api_bindings_client = std::make_unique<ApiBindingsClient>(
      std::move(api_bindings_client),
      base::BindOnce(&CastRunner::MaybeStartComponent, base::Unretained(this),
                     base::Unretained(pending_component.get())),
      base::BindOnce(&CastRunner::CancelComponentLaunch, base::Unretained(this),
                     base::Unretained(pending_component.get())));

  // Get UrlRequestRewriteRulesProvider from the Agent.
  fidl::InterfaceHandle<chromium::cast::UrlRequestRewriteRulesProvider>
      url_request_rules_provider;
  pending_component->agent_manager->ConnectToAgentService(
      kAgentComponentUrl, url_request_rules_provider.NewRequest());
  pending_component->rewrite_rules_provider = url_request_rules_provider.Bind();
  pending_component->rewrite_rules_provider.set_error_handler(
      [this, pending_component = pending_component.get()](zx_status_t status) {
        ZX_LOG(ERROR, status) << "UrlRequestRewriteRulesProvider disconnected.";
        CancelComponentLaunch(pending_component);
      });
  pending_component->rewrite_rules_provider->GetUrlRequestRewriteRules(
      [this, pending_component = pending_component.get()](
          std::vector<fuchsia::web::UrlRequestRewriteRule> rewrite_rules) {
        pending_component->rewrite_rules =
            base::Optional<std::vector<fuchsia::web::UrlRequestRewriteRule>>(
                std::move(rewrite_rules));
        MaybeStartComponent(pending_component);
      });

  // Request the configuration for the specified application.
  pending_component->agent_manager->ConnectToAgentService(
      kAgentComponentUrl, pending_component->app_config_manager.NewRequest());
  pending_component->app_config_manager.set_error_handler(
      [this, pending_component = pending_component.get()](zx_status_t status) {
        ZX_LOG(ERROR, status) << "ApplicationConfigManager disconnected.";
        CancelComponentLaunch(pending_component);
      });
  const std::string cast_app_id(cast_url.GetContent());
  pending_component->app_config_manager->GetConfig(
      cast_app_id, [this, pending_component = pending_component.get()](
                       chromium::cast::ApplicationConfig app_config) {
        GetConfigCallback(pending_component, std::move(app_config));
      });

  pending_components_.emplace(std::move(pending_component));
}

void CastRunner::DestroyComponent(WebComponent* component) {
  WebContentRunner::DestroyComponent(component);

  if (on_destruction_callback_) {
    // |this| may be deleted and should not be used after this line.
    std::move(on_destruction_callback_).Run(this);
    return;
  }
}

const char CastRunner::kAgentComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/cast_agent#meta/cast_agent.cmx";

void CastRunner::GetConfigCallback(
    CastComponent::CastComponentParams* pending_component,
    chromium::cast::ApplicationConfig app_config) {
  auto it = pending_components_.find(pending_component);
  DCHECK(it != pending_components_.end());

  if (app_config.IsEmpty()) {
    pending_components_.erase(it);
    DLOG(WARNING) << "No application config was found.";
    return;
  }

  if (!app_config.has_web_url()) {
    pending_components_.erase(it);
    DLOG(WARNING) << "Only web-based applications are supported.";
    return;
  }

  pending_component->app_config = std::move(app_config);

  MaybeStartComponent(pending_component);
}

void CastRunner::MaybeStartComponent(
    CastComponent::CastComponentParams* pending_component_params) {
  if (!AreCastComponentParamsValid(*pending_component_params))
    return;

  // The runner which will host the newly created CastComponent.
  CastRunner* component_owner = this;
  if (pending_component_params->app_config
          .has_content_directories_for_isolated_application()) {
    // Create a isolated, isolated CastRunner instance which will own the
    // CastComponent.
    component_owner =
        CreateChildRunnerForIsolatedComponent(pending_component_params);
  }

  component_owner->CreateAndRegisterCastComponent(
      std::move(*pending_component_params));
  pending_components_.erase(pending_component_params);
}

void CastRunner::CancelComponentLaunch(
    CastComponent::CastComponentParams* params) {
  size_t count = pending_components_.erase(params);
  DCHECK_EQ(count, 1u);
}

void CastRunner::CreateAndRegisterCastComponent(
    CastComponent::CastComponentParams params) {
  GURL app_url = GURL(params.app_config.web_url());
  auto cast_component =
      std::make_unique<CastComponent>(this, std::move(params));
  cast_component->StartComponent();
  cast_component->LoadUrl(std::move(app_url),
                          std::vector<fuchsia::net::http::Header>());
  RegisterComponent(std::move(cast_component));
}

CastRunner* CastRunner::CreateChildRunnerForIsolatedComponent(
    CastComponent::CastComponentParams* params) {
  // Construct the CreateContextParams in order to create a new Context.
  // Some common parameters must be inherited from
  // |common_create_context_params_|.
  fuchsia::web::CreateContextParams isolated_context_params;
  zx_status_t status =
      common_create_context_params_.Clone(&isolated_context_params);
  ZX_CHECK(status == ZX_OK, status) << "clone";
  isolated_context_params.set_service_directory(base::fuchsia::OpenDirectory(
      base::FilePath(base::fuchsia::kServiceDirectoryPath)));
  isolated_context_params.set_content_directories(
      std::move(*params->app_config
                     .mutable_content_directories_for_isolated_application()));

  std::unique_ptr<CastRunner> cast_runner(
      new CastRunner(base::BindOnce(&CastRunner::OnChildRunnerDestroyed,
                                    base::Unretained(this)),
                     CreateWebContext(std::move(isolated_context_params))));

  // If test code is listening for Component creation events, then wire up the
  // isolated CastRunner to signal component creation events.
  if (web_component_created_callback_for_test()) {
    cast_runner->SetWebComponentCreatedCallbackForTest(
        web_component_created_callback_for_test());
  }

  CastRunner* cast_runner_ptr = cast_runner.get();
  isolated_runners_.insert(std::move(cast_runner));
  return cast_runner_ptr;
}

void CastRunner::OnChildRunnerDestroyed(CastRunner* runner) {
  auto runner_iterator = isolated_runners_.find(runner);
  DCHECK(runner_iterator != isolated_runners_.end());

  isolated_runners_.erase(runner_iterator);
}

size_t CastRunner::GetChildCastRunnerCountForTest() {
  return isolated_runners_.size();
}
