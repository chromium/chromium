// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fit/function.h>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/logging.h"
#include "fuchsia/base/agent_manager.h"
#include "fuchsia/runners/cast/cast_streaming.h"
#include "fuchsia/runners/cast/pending_cast_component.h"
#include "fuchsia/runners/common/web_content_runner.h"
#include "url/gurl.h"

namespace {

// List of services provided to the WebEngine context.
// All services must be listed in cast_runner.cmx.
static constexpr const char* kServices[] = {
    "fuchsia.accessibility.semantics.SemanticsManager",
    "fuchsia.device.NameProvider",
    "fuchsia.fonts.Provider",
    "fuchsia.intl.PropertyProvider",
    "fuchsia.logger.LogSink",
    "fuchsia.media.SessionAudioConsumerFactory",
    "fuchsia.media.drm.PlayReady",
    "fuchsia.media.drm.Widevine",
    "fuchsia.mediacodec.CodecFactory",
    "fuchsia.memorypressure.Provider",
    "fuchsia.net.NameLookup",
    "fuchsia.netstack.Netstack",
    "fuchsia.posix.socket.Provider",
    "fuchsia.process.Launcher",
    "fuchsia.sysmem.Allocator",
    "fuchsia.ui.input.ImeService",
    "fuchsia.ui.input.ImeVisibilityService",
    "fuchsia.ui.scenic.Scenic",
    "fuchsia.vulkan.loader.Loader",

    // These services are redirected to the Agent:
    // * fuchsia.camera3.DeviceWatcher
    // * fuchsia.legacymetrics.MetricsRecorder
    // * fuchsia.media.Audio
};

bool IsPermissionGrantedInAppConfig(
    const chromium::cast::ApplicationConfig& application_config,
    fuchsia::web::PermissionType permission_type) {
  if (application_config.has_permissions()) {
    for (auto& permission : application_config.permissions()) {
      if (permission.has_type() && permission.type() == permission_type)
        return true;
    }
  }
  return false;
}

// Ephemeral remote debugging port used by child contexts.
const uint16_t kEphemeralRemoteDebuggingPort = 0;

}  // namespace

CastRunner::CastRunner(bool is_headless)
    : is_headless_(is_headless),
      main_services_(std::make_unique<base::fuchsia::FilteredServiceDirectory>(
          base::ComponentContextForProcess()->svc().get())),
      main_context_(std::make_unique<WebContentRunner>(
          base::BindRepeating(&CastRunner::GetMainContextParams,
                              base::Unretained(this)))),
      isolated_services_(
          std::make_unique<base::fuchsia::FilteredServiceDirectory>(
              base::ComponentContextForProcess()->svc().get())) {
  // Specify the services to connect via the Runner process' service directory.
  for (const char* name : kServices) {
    main_services_->AddService(name);
    isolated_services_->AddService(name);
  }

  // Add handlers to main context's service directory for redirected services.
  main_services_->outgoing_directory()->AddPublicService<fuchsia::media::Audio>(
      fit::bind_member(this, &CastRunner::OnAudioServiceRequest));
  main_services_->outgoing_directory()
      ->AddPublicService<fuchsia::camera3::DeviceWatcher>(
          fit::bind_member(this, &CastRunner::OnCameraServiceRequest));
  main_services_->outgoing_directory()
      ->AddPublicService<fuchsia::legacymetrics::MetricsRecorder>(
          fit::bind_member(this, &CastRunner::OnMetricsRecorderServiceRequest));

  // Isolated contexts can use the normal Audio service, and don't record
  // metrics.
  isolated_services_->AddService(fuchsia::media::Audio::Name_);
}

CastRunner::~CastRunner() = default;

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

  pending_components_.emplace(std::make_unique<PendingCastComponent>(
      this,
      std::make_unique<base::fuchsia::StartupContext>(std::move(startup_info)),
      std::move(controller_request), cast_url.GetContent()));
}

fuchsia::web::FrameHost* CastRunner::main_context_frame_host() const {
  return main_context_.get();
}

void CastRunner::LaunchPendingComponent(PendingCastComponent* pending_component,
                                        CastComponent::Params params) {
  // Save the list of CORS exemptions so that they can be used in Context
  // creation parameters.
  cors_exempt_headers_ = pending_component->TakeCorsExemptHeaders();

  // TODO(crbug.com/1082821): Remove |web_content_url| once the Cast Streaming
  // Receiver component has been implemented.
  GURL web_content_url(params.application_config.web_url());
  if (IsAppConfigForCastStreaming(params.application_config))
    web_content_url = GURL(kCastStreamingWebUrl);

  base::Optional<fuchsia::web::CreateContextParams> create_context_params =
      GetContextParamsForAppConfig(&params.application_config);

  WebContentRunner* component_owner = main_context_.get();
  if (create_context_params) {
    component_owner = CreateIsolatedContextForParams(
        std::move(create_context_params.value()));
  }

  auto cast_component = std::make_unique<CastComponent>(
      component_owner, std::move(params), is_headless_);

  // Start the component, which creates and configures the web.Frame, and load
  // the specified web content into it.
  cast_component->SetOnDestroyedCallback(
      base::BindOnce(&CastRunner::OnComponentDestroyed, base::Unretained(this),
                     base::Unretained(cast_component.get())));
  cast_component->StartComponent();
  cast_component->LoadUrl(std::move(web_content_url),
                          std::vector<fuchsia::net::http::Header>());

  if (component_owner == main_context_.get()) {
    // If this component has the microphone permission then use it to route
    // Audio service requests through.
    if (IsPermissionGrantedInAppConfig(
            cast_component->application_config(),
            fuchsia::web::PermissionType::MICROPHONE)) {
      audio_capturer_component_ = cast_component.get();
    }

    if (IsPermissionGrantedInAppConfig(cast_component->application_config(),
                                       fuchsia::web::PermissionType::CAMERA)) {
      video_capturer_component_ = cast_component.get();
    }
  }

  // Register the new component and clean up the |pending_component|.
  component_owner->RegisterComponent(std::move(cast_component));
  pending_components_.erase(pending_component);
}

void CastRunner::CancelPendingComponent(
    PendingCastComponent* pending_component) {
  size_t count = pending_components_.erase(pending_component);
  DCHECK_EQ(count, 1u);
}

void CastRunner::OnComponentDestroyed(CastComponent* component) {
  if (component == audio_capturer_component_)
    audio_capturer_component_ = nullptr;

  if (component == video_capturer_component_)
    video_capturer_component_ = nullptr;
}

fuchsia::web::CreateContextParams CastRunner::GetCommonContextParams() {
  fuchsia::web::CreateContextParams params;
  params.set_features(fuchsia::web::ContextFeatureFlags::AUDIO |
                      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM);

  if (is_headless_) {
    LOG(WARNING) << "Running in headless mode.";
    *params.mutable_features() |= fuchsia::web::ContextFeatureFlags::HEADLESS;
  } else {
    // TODO(crbug.com/1078227): Remove HARDWARE_VIDEO_DECODER_ONLY.
    *params.mutable_features() |=
        fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
        fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY |
        fuchsia::web::ContextFeatureFlags::VULKAN;
  }

  // TODO(b/154204041) Migrate to using persistent data, and specifying an
  // explicit quota for CDM storage.
  params.set_cdm_data_directory(base::fuchsia::OpenDirectory(
      base::FilePath(base::fuchsia::kPersistedCacheDirectoryPath)));
  CHECK(params.cdm_data_directory());

  const char kCastPlayreadyKeySystem[] = "com.chromecast.playready";
  params.set_playready_key_system(kCastPlayreadyKeySystem);

  // See http://b/141956135.
  params.set_user_agent_product("CrKey");
  params.set_user_agent_version("1.52.000000");

  // When tests require that VULKAN be disabled, DRM must also be disabled.
  if (disable_vulkan_for_test_) {
    *params.mutable_features() &=
        ~(fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM |
          fuchsia::web::ContextFeatureFlags::VULKAN |
          fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
          fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY);
    params.clear_playready_key_system();
  }

  // If there is a list of headers to exempt from CORS checks, pass the list
  // along to the Context.
  if (!cors_exempt_headers_.empty())
    params.set_cors_exempt_headers(cors_exempt_headers_);

  return params;
}

fuchsia::web::CreateContextParams CastRunner::GetMainContextParams() {
  fuchsia::web::CreateContextParams params = GetCommonContextParams();
  params.set_remote_debugging_port(CastRunner::kRemoteDebuggingPort);
  *params.mutable_features() |=
      fuchsia::web::ContextFeatureFlags::NETWORK |
      fuchsia::web::ContextFeatureFlags::LEGACYMETRICS;
  main_services_->ConnectClient(
      params.mutable_service_directory()->NewRequest());

  // TODO(crbug.com/1023514): Remove this switch when it is no longer
  // necessary.
  params.set_unsafely_treat_insecure_origins_as_secure(
      {"allow-running-insecure-content", "disable-mixed-content-autoupgrade"});

  return params;
}

fuchsia::web::CreateContextParams
CastRunner::GetIsolatedContextParamsWithFuchsiaDirs(
    std::vector<fuchsia::web::ContentDirectoryProvider> content_directories) {
  fuchsia::web::CreateContextParams params = GetCommonContextParams();
  params.set_remote_debugging_port(kEphemeralRemoteDebuggingPort);
  params.set_content_directories(std::move(content_directories));
  isolated_services_->ConnectClient(
      params.mutable_service_directory()->NewRequest());
  return params;
}

fuchsia::web::CreateContextParams
CastRunner::GetIsolatedContextParamsForCastStreaming() {
  fuchsia::web::CreateContextParams params = GetCommonContextParams();
  params.set_remote_debugging_port(kEphemeralRemoteDebuggingPort);
  ApplyCastStreamingContextParams(&params);
  // TODO(crbug.com/1069746): Use a different FilteredServiceDirectory for Cast
  // Streaming Contexts.
  main_services_->ConnectClient(
      params.mutable_service_directory()->NewRequest());
  return params;
}

base::Optional<fuchsia::web::CreateContextParams>
CastRunner::GetContextParamsForAppConfig(
    chromium::cast::ApplicationConfig* app_config) {
  base::Optional<fuchsia::web::CreateContextParams> params;

  if (IsAppConfigForCastStreaming(*app_config)) {
    // TODO(crbug.com/1082821): Remove this once the CastStreamingReceiver
    // Component has been implemented.
    return base::make_optional(GetIsolatedContextParamsForCastStreaming());
  }

  const bool is_isolated_app =
      app_config->has_content_directories_for_isolated_application();
  if (is_isolated_app) {
    return base::make_optional(
        GetIsolatedContextParamsWithFuchsiaDirs(std::move(
            *app_config
                 ->mutable_content_directories_for_isolated_application())));
  }

  // No need to create an isolated context in other cases.
  return base::nullopt;
}

WebContentRunner* CastRunner::CreateIsolatedContextForParams(
    fuchsia::web::CreateContextParams create_context_params) {
  // Create an isolated context which will own the CastComponent.
  auto context =
      std::make_unique<WebContentRunner>(std::move(create_context_params));
  context->SetOnEmptyCallback(
      base::BindOnce(&CastRunner::OnIsolatedContextEmpty,
                     base::Unretained(this), base::Unretained(context.get())));
  WebContentRunner* raw_context = context.get();
  isolated_contexts_.insert(std::move(context));
  return raw_context;
}

void CastRunner::OnIsolatedContextEmpty(WebContentRunner* context) {
  auto it = isolated_contexts_.find(context);
  DCHECK(it != isolated_contexts_.end());
  isolated_contexts_.erase(it);
}

void CastRunner::OnAudioServiceRequest(
    fidl::InterfaceRequest<fuchsia::media::Audio> request) {
  // If we have a component that allows AudioCapturer access then redirect the
  // fuchsia.media.Audio requests to the corresponding agent.
  if (audio_capturer_component_) {
    audio_capturer_component_->agent_manager()->ConnectToAgentService(
        audio_capturer_component_->application_config().agent_url(),
        std::move(request));
    return;
  }

  // Otherwise use the Runner's fuchsia.media.Audio service. fuchsia.media.Audio
  // may be used by frames without MICROPHONE permission to create AudioRenderer
  // instance.
  base::ComponentContextForProcess()->svc()->Connect(std::move(request));
}

void CastRunner::OnCameraServiceRequest(
    fidl::InterfaceRequest<fuchsia::camera3::DeviceWatcher> request) {
  // If we have a component that allows camera access then redirect the
  // fuchsia.camera3.DeviceWatcher requests to the corresponding agent.
  if (video_capturer_component_) {
    video_capturer_component_->agent_manager()->ConnectToAgentService(
        video_capturer_component_->application_config().agent_url(),
        std::move(request));
    return;
  }

  // fuchsia.camera3.DeviceWatcher may be requested while none of the running
  // apps have the CAMERA permission. Return ZX_ERR_UNAVAILABLE, which implies
  // that the client should try connecting again later, since the service may
  // become available after a web.Frame with camera access is created.
  request.Close(ZX_ERR_UNAVAILABLE);
}

void CastRunner::OnMetricsRecorderServiceRequest(
    fidl::InterfaceRequest<fuchsia::legacymetrics::MetricsRecorder> request) {
  // TODO(https://crbug.com/1065707): Remove this hack once Runners are using
  // Component Framework v2.
  CastComponent* component =
      reinterpret_cast<CastComponent*>(main_context_->GetAnyComponent());
  DCHECK(component);

  component->startup_context()->svc()->Connect(std::move(request));
}
