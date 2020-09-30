// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_
#define FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_

#include <chromium/cast/cpp/fidl.h>
#include <fuchsia/camera3/cpp/fidl.h>
#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/fuchsia/startup_context.h"
#include "fuchsia/runners/cast/cast_component.h"
#include "fuchsia/runners/cast/pending_cast_component.h"

namespace base {
namespace fuchsia {
class FilteredServiceDirectory;
}  // namespace fuchsia
}  // namespace base

class WebContentRunner;

// sys::Runner which instantiates Cast activities specified via cast/casts URIs.
class CastRunner : public fuchsia::sys::Runner,
                   public PendingCastComponent::Delegate {
 public:
  static constexpr uint16_t kRemoteDebuggingPort = 9222;

  // Creates the Runner for Cast components.
  // |is_headless|: True if this instance should create Contexts with the
  //                HEADLESS feature set.
  explicit CastRunner(bool is_headless);
  ~CastRunner() final;

  CastRunner(const CastRunner&) = delete;
  CastRunner& operator=(const CastRunner&) = delete;

  // fuchsia::sys::Runner implementation.
  void StartComponent(fuchsia::sys::Package package,
                      fuchsia::sys::StartupInfo startup_info,
                      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                          controller_request) final;

  // Returns a fuchsia.web.FrameHost interface to the main web.Context used to
  // host non-isolated Cast applications.
  fuchsia::web::FrameHost* main_context_frame_host() const;

  // Disables use of the VULKAN feature when creating Contexts. Must be set
  // before calling StartComponent().
  void set_disable_vulkan_for_test() { disable_vulkan_for_test_ = true; }

 private:
  // PendingCastComponent::Delegate implementation.
  void LaunchPendingComponent(PendingCastComponent* pending_component,
                              CastComponent::Params params) final;
  void CancelPendingComponent(PendingCastComponent* pending_component) final;

  // Handles component destruction.
  void OnComponentDestroyed(CastComponent* component);

  // Handlers used to provide parameters for main & isolated Contexts.
  fuchsia::web::CreateContextParams GetCommonContextParams();
  fuchsia::web::CreateContextParams GetMainContextParams();
  fuchsia::web::CreateContextParams GetIsolatedContextParamsWithFuchsiaDirs(
      std::vector<fuchsia::web::ContentDirectoryProvider> content_directories);
  // TODO(crbug.com/1082821): Remove this once the CastStreamingReceiver
  // Component has been implemented.
  fuchsia::web::CreateContextParams GetIsolatedContextParamsForCastStreaming();

  // Returns CreateContextParams for |app_config|. Returns nullopt if there is
  // no need to create an isolated context.
  base::Optional<fuchsia::web::CreateContextParams>
  GetContextParamsForAppConfig(chromium::cast::ApplicationConfig* app_config);

  // Launches an isolated Context with the given |create_context_params| and
  // returns the newly created WebContentRunner.
  WebContentRunner* CreateIsolatedContextForParams(
      fuchsia::web::CreateContextParams create_context_params);

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

  // True if this Runner uses Context(s) with the HEADLESS feature set.
  const bool is_headless_;

  // Holds the main fuchsia.web.Context used to host CastComponents.
  // Note that although |main_context_| is actually a WebContentRunner, that is
  // only being used to maintain the Context for the hosted components.
  const std::unique_ptr<base::fuchsia::FilteredServiceDirectory> main_services_;
  const std::unique_ptr<WebContentRunner> main_context_;

  const std::unique_ptr<base::fuchsia::FilteredServiceDirectory>
      isolated_services_;

  // Holds fuchsia.web.Contexts used to host isolated components.
  base::flat_set<std::unique_ptr<WebContentRunner>, base::UniquePtrComparator>
      isolated_contexts_;

  // Temporarily holds a PendingCastComponent instance, responsible for fetching
  // the parameters required to launch the component, for each call to
  // StartComponent().
  base::flat_set<std::unique_ptr<PendingCastComponent>,
                 base::UniquePtrComparator>
      pending_components_;

  // List of HTTP headers to exempt from CORS checks.
  std::vector<std::vector<uint8_t>> cors_exempt_headers_;

  // Last component that was created with permission to access MICROPHONE.
  CastComponent* audio_capturer_component_ = nullptr;

  // Last component that was created with permission to access CAMERA.
  CastComponent* video_capturer_component_ = nullptr;

  // True if Contexts should be created without VULKAN set.
  bool disable_vulkan_for_test_ = false;
};

#endif  // FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_
