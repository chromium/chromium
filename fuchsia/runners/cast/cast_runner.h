// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_
#define FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_

#include <fuchsia/camera3/cpp/fidl.h>
#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/fuchsia/startup_context.h"
#include "fuchsia/runners/cast/cast_component.h"
#include "fuchsia/runners/cast/fidl/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/pending_cast_component.h"
#include "fuchsia/runners/common/web_content_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilteredServiceDirectory;
}  // namespace base

namespace cr_fuchsia {
class WebInstanceHost;
}  // namespace cr_fuchsia

// sys::Runner which instantiates Cast activities specified via cast/casts URIs.
class CastRunner final : public fuchsia::sys::Runner,
                         public PendingCastComponent::Delegate {
 public:
  static constexpr uint16_t kRemoteDebuggingPort = 9222;

  // Creates the Runner for Cast components.
  // |web_instance_host|: Used to create an isolated web_instance
  //     Component in which to host the fuchsia.web.Context.
  // |is_headless|: True if this instance should create Contexts with the
  //                HEADLESS feature set.
  CastRunner(cr_fuchsia::WebInstanceHost* web_instance_host, bool is_headless);
  ~CastRunner() override;

  CastRunner(const CastRunner&) = delete;
  CastRunner& operator=(const CastRunner&) = delete;

  // fuchsia::sys::Runner implementation.
  void StartComponent(fuchsia::sys::Package package,
                      fuchsia::sys::StartupInfo startup_info,
                      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                          controller_request) override;

  // Enables the special component that provides the fuchsia.web.FrameHost API,
  // hosted using the same WebEngine instance as the main web.Context.
  void set_enable_frame_host_component() {
    enable_frame_host_component_ = true;
  }

  // Disables use of the VULKAN feature when creating Contexts. Must be set
  // before calling StartComponent().
  void set_disable_vulkan_for_test() { disable_vulkan_for_test_ = true; }

 private:
  // PendingCastComponent::Delegate implementation.
  void LaunchPendingComponent(PendingCastComponent* pending_component,
                              CastComponent::Params params) override;
  void CancelPendingComponent(PendingCastComponent* pending_component) override;

  // Handles component destruction.
  void OnComponentDestroyed(CastComponent* component);

  // Handlers used to provide parameters for main & isolated Contexts.
  WebContentRunner::WebInstanceConfig GetCommonWebInstanceConfig();
  WebContentRunner::WebInstanceConfig GetMainWebInstanceConfig();
  WebContentRunner::WebInstanceConfig
  GetIsolatedWebInstanceConfigWithFuchsiaDirs(
      std::vector<fuchsia::web::ContentDirectoryProvider> content_directories);
  // TODO(crbug.com/1082821): Remove this once the CastStreamingReceiver
  // Component has been implemented.
  WebContentRunner::WebInstanceConfig
  GetIsolatedWebInstanceConfigForCastStreaming();

  // Returns CreateContextParams for |app_config|. Returns nullopt if there is
  // no need to create an isolated context.
  absl::optional<WebContentRunner::WebInstanceConfig>
  GetWebInstanceConfigForAppConfig(
      chromium::cast::ApplicationConfig* app_config);

  // Launches an isolated Context with the given `config` and returns the newly
  // created WebContentRunner.
  WebContentRunner* CreateIsolatedRunner(
      WebContentRunner::WebInstanceConfig config);

  // Called when an isolated component terminates, to allow the Context hosting
  // it to be torn down.
  void OnIsolatedContextEmpty(WebContentRunner* context);

  // Connection handlers for redirected services.
  void OnAudioServiceRequest(
      fidl::InterfaceRequest<fuchsia::media::Audio> request);
  void OnCameraServiceRequest(
      fidl::InterfaceRequest<fuchsia::camera3::DeviceWatcher> request);
  void OnMetricsRecorderServiceRequest(
      fidl::InterfaceRequest<fuchsia::legacymetrics::MetricsRecorder> request);

  // Internal implementation of StartComponent(), called after validating the
  // component URL and ensuring that CORS-exempt headers have been fetched.
  void StartComponentInternal(
      const GURL& url,
      std::unique_ptr<base::StartupContext> startup_context,
      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
          controller_request);

  // Moves all data persisted by the main Context to a staging directory,
  // which will be deleted the next time the Runner starts up.
  // Requests to launch new components in the main Context will be rejected
  // until this Runner instance is shutdown.
  // Returns true on success and false in case of I/O error.
  bool DeletePersistentData();

  // TODO(crbug.com/1188780): Used to detect when the persisted cache directory
  // was erased. The sentinel file is created at the top-level of the cache
  // directory, so cannot be deleted by the Context, only by the cache being
  // erased.
  void CreatePersistedCacheSentinel();
  bool WasPersistedCacheErased();

  // Passed to WebContentRunners to use to create web_instance Components.
  cr_fuchsia::WebInstanceHost* const web_instance_host_;

  // True if this Runner uses Context(s) with the HEADLESS feature set.
  const bool is_headless_;

  // Holds the main fuchsia.web.Context used to host CastComponents.
  // Note that although |main_context_| is actually a WebContentRunner, that is
  // only being used to maintain the Context for the hosted components.
  const std::unique_ptr<base::FilteredServiceDirectory> main_services_;
  const std::unique_ptr<WebContentRunner> main_context_;

  const std::unique_ptr<base::FilteredServiceDirectory> isolated_services_;

  // Holds fuchsia.web.Contexts used to host isolated components.
  base::flat_set<std::unique_ptr<WebContentRunner>, base::UniquePtrComparator>
      isolated_contexts_;

  // Temporarily holds a PendingCastComponent instance, responsible for fetching
  // the parameters required to launch the component, for each call to
  // StartComponent().
  base::flat_set<std::unique_ptr<PendingCastComponent>,
                 base::UniquePtrComparator>
      pending_components_;

  // True if this Runner should offer the fuchsia.web.FrameHost component.
  bool enable_frame_host_component_ = false;

  // Used to fetch & cache the list of CORS exempt HTTP headers to configure
  // each web.Context with.
  absl::optional<std::vector<std::vector<uint8_t>>> cors_exempt_headers_;
  chromium::cast::CorsExemptHeaderProviderPtr cors_exempt_headers_provider_;
  std::vector<base::OnceClosure> on_have_cors_exempt_headers_;

  // Reference to the service directory of the most recent FrameHost component.
  // Used to route MetricsRecorder requests from the web.Context, if there are
  // no CastComponents available through which to do so.
  base::WeakPtr<const sys::ServiceDirectory>
      frame_host_component_incoming_services_;

  // List of components created with permission to access MICROPHONE.
  base::flat_set<CastComponent*> audio_capturer_components_;

  // List of components created with permission to access CAMERA.
  base::flat_set<CastComponent*> video_capturer_components_;

  // The URL of the agent first using the respective capturer component.
  std::string first_audio_capturer_agent_url_;
  std::string first_video_capturer_agent_url_;

  // True if Contexts should be created without VULKAN set.
  bool disable_vulkan_for_test_ = false;

  // True if cast runner entered data reset mode. Prevents new components
  // in the main context from being launched. This is set to true once data
  // reset starts and does not switch back to false upon completion.
  bool data_reset_in_progress_ = false;

  // True if the cache sentinel file should exist.
  // TODO(crbug.com/1188780): Remove once an explicit cache flush signal exists.
  bool was_cache_sentinel_created_ = false;
};

#endif  // FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_
