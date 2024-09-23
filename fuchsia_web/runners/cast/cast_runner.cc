// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/cast_runner.h"

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fit/function.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/cast/common/constants.h"
#include "components/fuchsia_component_support/config_reader.h"
#include "fuchsia_web/cast_streaming/cast_streaming.h"
#include "fuchsia_web/runners/cast/cast_streaming.h"
#include "fuchsia_web/runners/cast/pending_cast_component.h"
#include "fuchsia_web/runners/common/web_content_runner.h"
#include "fuchsia_web/webinstance_host/web_instance_host.h"
#include "url/gurl.h"

namespace {

// Use a constexpr instead of the existing switch, because of the additional
// dependencies required.
constexpr char kAudioCapturerWithEchoCancellationSwitch[] =
    "audio-capturer-with-echo-cancellation";

// Names used to partition the Runner's persistent storage for different uses.
constexpr char kCdmDataSubdirectoryName[] = "cdm_data";
constexpr char kProfileSubdirectoryName[] = "web_profile";

// Name of the file used to detect cache erasure.
// TODO(crbug.com/40755074): Remove once an explicit cache flush signal exists.
constexpr char kSentinelFileName[] = ".sentinel";

// Ephemeral remote debugging port used by child contexts.
const uint16_t kEphemeralRemoteDebuggingPort = 0;

// Subdirectory used to stage persistent directories to be deleted upon next
// startup.
const char kStagedForDeletionSubdirectory[] = "staged_for_deletion";

base::FilePath GetStagedForDeletionDirectoryPath() {
  base::FilePath cache_directory(base::kPersistedCacheDirectoryPath);
  return cache_directory.Append(kStagedForDeletionSubdirectory);
}

// Deletes files/directories staged for deletion during the previous run.
// We delete synchronously on main thread for simplicity. Note that this
// overall mechanism is a temporary solution. TODO(crbug.com/40730097): migrate
// to the framework mechanism of clearing session data when available.
void DeleteStagedForDeletionDirectoryIfExists() {
  const base::FilePath staged_for_deletion_directory =
      GetStagedForDeletionDirectoryPath();

  if (!PathExists(staged_for_deletion_directory))
    return;

  const base::TimeTicks started_at = base::TimeTicks::Now();
  bool result = base::DeletePathRecursively(staged_for_deletion_directory);
  if (!result) {
    LOG(ERROR) << "Failed to delete the staging directory";
    return;
  }

  LOG(WARNING) << "Deleting old persistent data took "
               << (base::TimeTicks::Now() - started_at).InMillisecondsF()
               << " ms";
}

// TODO(crbug.com/40151573): Consider removing this flag once Media Capabilities
// is supported.
void EnsureSoftwareVideoDecodersAreDisabled(
    ::fuchsia::web::ContextFeatureFlags* features) {
  if ((*features & fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER) ==
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER) {
    *features |= fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY;
  }
}

// Populates |params| with web data settings. Web data persistence is only
// enabled if a soft quota is explicitly specified via config-data.
// Exits the Runner process if creation of data storage fails for any reason.
void SetDataParamsForMainContext(fuchsia::web::CreateContextParams* params) {
  // Set the web data quota based on the CastRunner configuration.
  const std::optional<base::Value::Dict>& config =
      fuchsia_component_support::LoadPackageConfig();
  if (!config)
    return;

  constexpr char kDataQuotaBytesSwitch[] = "data-quota-bytes";
  const std::optional<int> data_quota_bytes =
      config->FindInt(kDataQuotaBytesSwitch);
  if (!data_quota_bytes)
    return;

  // Allow best-effort persistent of Cast application data.
  // TODO(crbug.com/42050202): Remove the need for an explicit quota to be
  // configured, once the platform provides storage quotas.
  const auto profile_path = base::FilePath(base::kPersistedCacheDirectoryPath)
                                .Append(kProfileSubdirectoryName);
  auto error_code = base::File::Error::FILE_OK;
  if (!base::CreateDirectoryAndGetError(profile_path, &error_code)) {
    LOG(ERROR) << "Failed to create profile directory: " << error_code;
    base::Process::TerminateCurrentProcessImmediately(1);
  }
  params->set_data_directory(base::OpenDirectoryHandle(profile_path));
  if (!params->data_directory()) {
    LOG(ERROR) << "Unable to open data to transfer";
    base::Process::TerminateCurrentProcessImmediately(1);
  }
  params->set_data_quota_bytes(*data_quota_bytes);
}

// Populates |params| with settings to enable Widevine & PlayReady CDMs.
// CDM data persistence is always enabled, with an optional soft quota.
// Exits the Runner if creation of CDM storage fails for any reason.
void SetCdmParamsForMainContext(fuchsia::web::CreateContextParams* params) {
  const std::optional<base::Value::Dict>& config =
      fuchsia_component_support::LoadPackageConfig();
  if (config) {
    constexpr char kCdmDataQuotaBytesSwitch[] = "cdm-data-quota-bytes";
    const std::optional<int> cdm_data_quota_bytes =
        config->FindInt(kCdmDataQuotaBytesSwitch);
    if (cdm_data_quota_bytes)
      params->set_cdm_data_quota_bytes(*cdm_data_quota_bytes);
  }

  // TODO(b/154204041): Consider using isolated-persistent-storage for CDM data.
  // Create an isolated-cache-storage sub-directory for CDM data.
  const auto cdm_data_path = base::FilePath(base::kPersistedCacheDirectoryPath)
                                 .Append(kCdmDataSubdirectoryName);
  auto error_code = base::File::Error::FILE_OK;
  if (!base::CreateDirectoryAndGetError(cdm_data_path, &error_code)) {
    LOG(ERROR) << "Failed to create cache directory: " << error_code;
    base::Process::TerminateCurrentProcessImmediately(1);
  }
  params->set_cdm_data_directory(base::OpenDirectoryHandle(cdm_data_path));
  if (!params->cdm_data_directory()) {
    LOG(ERROR) << "Unable to open cdm_data to transfer";
    base::Process::TerminateCurrentProcessImmediately(1);
  }

  // Enable the Widevine and Playready CDMs.
  *params->mutable_features() |=
      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM;
  const char kCastPlayreadyKeySystem[] = "com.chromecast.playready";
  params->set_playready_key_system(kCastPlayreadyKeySystem);
}

}  // namespace

CastRunner::CastRunner(WebInstanceHost& web_instance_host, Options options)
    : web_instance_host_(web_instance_host),
      is_headless_(options.headless),
      disable_codegen_(options.disable_codegen),
      main_context_(std::make_unique<WebContentRunner>(
          base::BindRepeating(
              &WebInstanceHost::CreateInstanceForContextWithCopiedArgs,
              base::Unretained(&web_instance_host_.get())),
          base::BindRepeating(&CastRunner::GetMainWebInstanceConfig,
                              base::Unretained(this)))) {
  // Delete persisted data staged for deletion during the previous run.
  DeleteStagedForDeletionDirectoryIfExists();

  // Fetch the list of CORS-exempt headers to apply for all components launched
  // under this Runner.
  zx_status_t status = base::ComponentContextForProcess()->svc()->Connect(
      cors_exempt_headers_provider_.NewRequest());
  ZX_CHECK(status == ZX_OK, status)
      << "Connect CorsExemptHeaderProvider failed";
  cors_exempt_headers_provider_.set_error_handler(
      base::LogFidlErrorAndExitProcess(FROM_HERE, "CorsExemptHeaderProvider"));
  cors_exempt_headers_provider_->GetCorsExemptHeaderNames(
      [this](std::vector<std::vector<uint8_t>> header_names) {
        cors_exempt_headers_provider_.Unbind();
        cors_exempt_headers_ = std::move(header_names);
        for (auto& callback : on_have_cors_exempt_headers_)
          std::move(callback).Run();
        on_have_cors_exempt_headers_.clear();
      });
}

CastRunner::~CastRunner() = default;

void CastRunner::Start(
    fuchsia::component::runner::ComponentStartInfo start_info,
    fidl::InterfaceRequest<fuchsia::component::runner::ComponentController>
        controller_request) {
  // Verify that |package| specifies a Cast URI, and pull the app-Id from it.
  constexpr char kCastPresentationUrlScheme[] = "cast";
  constexpr char kCastSecurePresentationUrlScheme[] = "casts";

  GURL cast_url(start_info.has_resolved_url() ? start_info.resolved_url() : "");
  if (!cast_url.is_valid() ||
      (!cast_url.SchemeIs(kCastPresentationUrlScheme) &&
       !cast_url.SchemeIs(kCastSecurePresentationUrlScheme)) ||
      cast_url.GetContent().empty()) {
    LOG(ERROR) << "Rejected invalid URL: " << cast_url;
    return;
  }

  auto startup_context =
      std::make_unique<base::StartupContext>(std::move(start_info));

  // If the persistent cache directory was erased then re-create the main Cast
  // app Context.
  if (WasPersistedCacheErased()) {
    LOG(WARNING) << "Cache erased. Restarting web.Context.";
    // The sentinel file will be re-created the next time CreateContextParams
    // are request for the main web.Context.
    was_cache_sentinel_created_ = false;
    main_context_->DestroyWebContext();
  }

  if (cors_exempt_headers_) {
    StartComponentInternal(cast_url, std::move(startup_context),
                           std::move(controller_request));
  } else {
    // Queue the component launch to be resumed once the header list is
    // available.
    on_have_cors_exempt_headers_.push_back(base::BindOnce(
        &CastRunner::StartComponentInternal, base::Unretained(this), cast_url,
        std::move(startup_context), std::move(controller_request)));
  }
}

void CastRunner::DeletePersistentData(DeletePersistentDataCallback callback) {
  if (data_reset_in_progress_) {
    // Repeated requests to DeletePersistentData are not supported.
    // It is expected that the Runner be requested to terminate shortly after
    // data-reset is received, in any case.
    callback(false);
    return;
  }

  callback(DeletePersistentDataInternal());
}

void CastRunner::LaunchPendingComponent(PendingCastComponent* pending_component,
                                        CastComponent::Params params) {
  DCHECK(cors_exempt_headers_);

  // TODO(crbug.com/40131115): Remove |web_content_url| once the Cast Streaming
  // Receiver component has been implemented.
  GURL web_content_url(params.application_config.web_url());
  if (IsAppConfigForCastStreaming(params.application_config))
    web_content_url = GURL(kCastStreamingWebUrl);

  auto web_instance_config =
      GetWebInstanceConfigForAppConfig(&params.application_config);

  WebContentRunner* component_owner = main_context_.get();
  if (web_instance_config) {
    component_owner =
        CreateIsolatedRunner(std::move(web_instance_config.value()));
  }

  auto cast_component = std::make_unique<CastComponent>(
      base::StrCat({"cast:", pending_component->app_id()}), component_owner,
      std::move(params), is_headless_);

  // Start the component, which creates and configures the web.Frame, and load
  // the specified web content into it.
  cast_component->StartComponent();
  cast_component->LoadUrl(std::move(web_content_url),
                          std::vector<fuchsia::net::http::Header>());

  if (component_owner == main_context_.get()) {
    // For components in the main Context the cache sentinel file should have
    // been created as a side-effect of |CastComponent::StartComponent()|.
    DCHECK(was_cache_sentinel_created_);
  }

  // Do not launch new main context components while data reset is in progress,
  // so that they don't create new persisted state. We expect the session
  // to be restarted shortly after data reset completes.
  if (data_reset_in_progress_ && component_owner == main_context_.get()) {
    pending_components_.erase(pending_component);
    return;
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

WebContentRunner::WebInstanceConfig CastRunner::GetCommonWebInstanceConfig() {
  DCHECK(cors_exempt_headers_);

  WebContentRunner::WebInstanceConfig config;

  static constexpr char const* kSwitchesToCopy[] = {
      // Must match the value in `content/public/common/content_switches.cc`.
      "enable-logging",
      // Must match the value in `ui/ozone/public/ozone_switches.cc`.
      "ozone-platform",
  };
  config.extra_args.CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                     kSwitchesToCopy);

  config.params.set_features(fuchsia::web::ContextFeatureFlags::AUDIO);

  if (is_headless_) {
    LOG(WARNING) << "Running in headless mode.";
    *config.params.mutable_features() |=
        fuchsia::web::ContextFeatureFlags::HEADLESS;
  } else {
    *config.params.mutable_features() |=
        fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
        fuchsia::web::ContextFeatureFlags::VULKAN;
  }

  if (disable_codegen_) {
    *config.params.mutable_features() |=
        fuchsia::web::ContextFeatureFlags::DISABLE_DYNAMIC_CODE_GENERATION;
  }

  // When tests require that VULKAN be disabled, DRM must also be disabled.
  if (disable_vulkan_for_test_) {
    *config.params.mutable_features() &=
        ~(fuchsia::web::ContextFeatureFlags::VULKAN |
          fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER);
  }

  // If there is a list of headers to exempt from CORS checks, pass the list
  // along to the Context.
  if (!cors_exempt_headers_->empty())
    config.params.set_cors_exempt_headers(*cors_exempt_headers_);

  return config;
}

WebContentRunner::WebInstanceConfig CastRunner::GetMainWebInstanceConfig() {
  auto config = GetCommonWebInstanceConfig();

  // The fuchsia.media.Audio implementation provided to the Runner in existing
  // integrations always supports echo cancellation.
  //
  // TODO(crbug.com/42050621): Remove once AudioManagerFuchsia is updated to
  // get this information from AudioCapturerFactory.
  config.extra_args.AppendSwitch(kAudioCapturerWithEchoCancellationSwitch);

  *config.params.mutable_features() |=
      fuchsia::web::ContextFeatureFlags::NETWORK |
      fuchsia::web::ContextFeatureFlags::LEGACYMETRICS |
      fuchsia::web::ContextFeatureFlags::KEYBOARD |
      fuchsia::web::ContextFeatureFlags::VIRTUAL_KEYBOARD;
  EnsureSoftwareVideoDecodersAreDisabled(config.params.mutable_features());
  config.params.set_remote_debugging_port(CastRunner::kRemoteDebuggingPort);

  config.params.set_user_agent_product("CrKey");
  config.params.set_user_agent_version(chromecast::kFrozenCrKeyValue);

  if (!disable_vulkan_for_test_) {
    SetCdmParamsForMainContext(&config.params);
  }

  SetDataParamsForMainContext(&config.params);

  // Create a sentinel file to detect if the cache is erased.
  // TODO(crbug.com/40755074): Remove once an explicit cache flush signal
  // exists.
  CreatePersistedCacheSentinel();

  // TODO(crbug.com/40050660): Remove this switch when it is no longer
  // necessary.
  config.params.set_unsafely_treat_insecure_origins_as_secure(
      {"allow-running-insecure-content", "disable-mixed-content-autoupgrade"});

  return config;
}

WebContentRunner::WebInstanceConfig
CastRunner::GetIsolatedWebInstanceConfigWithFuchsiaDirs(
    std::vector<fuchsia::web::ContentDirectoryProvider> content_directories) {
  auto config = GetCommonWebInstanceConfig();

  EnsureSoftwareVideoDecodersAreDisabled(config.params.mutable_features());
  *config.params.mutable_features() |=
      fuchsia::web::ContextFeatureFlags::NETWORK;
  config.params.set_remote_debugging_port(kEphemeralRemoteDebuggingPort);
  config.params.set_content_directories(std::move(content_directories));

  return config;
}

WebContentRunner::WebInstanceConfig
CastRunner::GetIsolatedWebInstanceConfigForCastStreaming() {
  auto config = GetCommonWebInstanceConfig();

  ApplyCastStreamingContextParams(&config.params);
  config.params.set_remote_debugging_port(kEphemeralRemoteDebuggingPort);

  return config;
}

std::optional<WebContentRunner::WebInstanceConfig>
CastRunner::GetWebInstanceConfigForAppConfig(
    chromium::cast::ApplicationConfig* app_config) {
  if (IsAppConfigForCastStreaming(*app_config)) {
    // TODO(crbug.com/40131115): Remove this once the CastStreamingReceiver
    // Component has been implemented.
    return std::make_optional(GetIsolatedWebInstanceConfigForCastStreaming());
  }

  const bool is_isolated_app =
      app_config->has_content_directories_for_isolated_application();
  if (is_isolated_app) {
    return std::make_optional(
        GetIsolatedWebInstanceConfigWithFuchsiaDirs(std::move(
            *app_config
                 ->mutable_content_directories_for_isolated_application())));
  }

  // No need to create an isolated context in other cases.
  return std::nullopt;
}

WebContentRunner* CastRunner::CreateIsolatedRunner(
    WebContentRunner::WebInstanceConfig config) {
  // Create an isolated context which will own the CastComponent.
  auto context = std::make_unique<WebContentRunner>(
      base::BindRepeating(
          &WebInstanceHost::CreateInstanceForContextWithCopiedArgs,
          base::Unretained(&web_instance_host_.get())),
      std::move(config));
  context->SetOnEmptyCallback(
      base::BindOnce(&CastRunner::OnIsolatedContextEmpty,
                     base::Unretained(this), base::Unretained(context.get())));
  WebContentRunner* raw_context = context.get();
  isolated_contexts_.insert(std::move(context));
  return raw_context;
}

void CastRunner::OnIsolatedContextEmpty(WebContentRunner* context) {
  auto it = isolated_contexts_.find(context);
  CHECK(it != isolated_contexts_.end(), base::NotFatalUntil::M130);
  isolated_contexts_.erase(it);
}

void CastRunner::StartComponentInternal(
    const GURL& url,
    std::unique_ptr<base::StartupContext> startup_context,
    fidl::InterfaceRequest<fuchsia::component::runner::ComponentController>
        controller_request) {
  pending_components_.emplace(std::make_unique<PendingCastComponent>(
      this, std::move(startup_context), std::move(controller_request),
      url.GetContent()));
}

static base::FilePath SentinelFilePath() {
  return base::FilePath(base::kPersistedCacheDirectoryPath)
      .Append(kSentinelFileName);
}

bool CastRunner::DeletePersistentDataInternal() {
  // Set data reset flag so that new components are not being started.
  data_reset_in_progress_ = true;

  // Create the staging directory.
  base::FilePath staged_for_deletion_directory =
      GetStagedForDeletionDirectoryPath();
  base::File::Error file_error;
  bool result = base::CreateDirectoryAndGetError(staged_for_deletion_directory,
                                                 &file_error);
  if (!result) {
    LOG(ERROR) << "Failed to create the staging directory, error: "
               << file_error;
    return false;
  }

  // Stage everything under `/cache` for deletion.
  const base::FilePath cache_directory(base::kPersistedCacheDirectoryPath);
  base::FileEnumerator enumerator(
      cache_directory, /*recursive=*/false,
      base::FileEnumerator::FileType::FILES |
          base::FileEnumerator::FileType::DIRECTORIES);
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    // Skip the staging directory itself.
    if (current == staged_for_deletion_directory) {
      continue;
    }

    base::FilePath destination =
        staged_for_deletion_directory.Append(current.BaseName());
    result = base::Move(current, destination);
    if (!result) {
      LOG(ERROR) << "Failed to move " << current << " to " << destination;
      return false;
    }
  }

  return true;
}

fidl::InterfaceRequestHandler<fuchsia::web::FrameHost>
CastRunner::GetFrameHostRequestHandler() {
  return [this](fidl::InterfaceRequest<fuchsia::web::FrameHost> request) {
    if (!cors_exempt_headers_) {
      on_have_cors_exempt_headers_.push_back(base::BindOnce(
          [](fidl::InterfaceRequestHandler<fuchsia::web::FrameHost>
                 request_handler,
             fidl::InterfaceRequest<fuchsia::web::FrameHost> request) {
            request_handler(std::move(request));
          },
          main_context_->GetFrameHostRequestHandler(), std::move(request)));
      return;
    }
    main_context_->GetFrameHostRequestHandler()(std::move(request));
  };
}

void CastRunner::CreatePersistedCacheSentinel() {
  base::WriteFile(SentinelFilePath(), "");
  was_cache_sentinel_created_ = true;
}

bool CastRunner::WasPersistedCacheErased() {
  if (!was_cache_sentinel_created_)
    return false;
  return !base::PathExists(SentinelFilePath());
}
