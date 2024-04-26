// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_CAST_RUNNER_H_
#define FUCHSIA_WEB_RUNNERS_CAST_CAST_RUNNER_H_

#include <chromium/cast/cpp/fidl.h>
#include <fuchsia/component/runner/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/fuchsia/startup_context.h"
#include "base/functional/callback.h"
#include "fuchsia_web/runners/cast/cast_component.h"
#include "fuchsia_web/runners/cast/pending_cast_component.h"
#include "fuchsia_web/runners/common/web_content_runner.h"

class WebInstanceHost;

// ComponentRunner that runs Cast activities specified via cast/casts URIs.
class CastRunner final : public fuchsia::component::runner::ComponentRunner,
                         public chromium::cast::DataReset,
                         public PendingCastComponent::Delegate {
 public:
  struct Options {
    // Set to true to run components without generating output via Scenic.
    bool headless = false;

    // Set to true to run components without without web optimizations (e.g.
    // JavaScript Just-In-Time compilation) or features (e.g. WebAssembly) that
    // require dynamic code generation.
    bool disable_codegen = false;
  };

  static constexpr uint16_t kRemoteDebuggingPort = 9222;

  // Creates the Runner for Cast components.
  // `web_instance_host` is used to create a "main" instance to host Cast apps
  // and serve `FrameHost` instances, and isolated containers for apps that
  // need them.
  CastRunner(WebInstanceHost& web_instance_host, Options options);
  ~CastRunner() override;

  CastRunner(const CastRunner&) = delete;
  CastRunner& operator=(const CastRunner&) = delete;

  // fuchsia::component::runner::ComponentRunner implementation.
  void Start(
      fuchsia::component::runner::ComponentStartInfo start_info,
      fidl::InterfaceRequest<fuchsia::component::runner::ComponentController>
          controller) override;

  // chromium::cast::DataReset implementation.
  void DeletePersistentData(DeletePersistentDataCallback callback) override;

  // Returns a connection request handler for the fuchsia.web.FrameHost
  // protocol exposed by the main web_instance.
  fidl::InterfaceRequestHandler<fuchsia::web::FrameHost>
  GetFrameHostRequestHandler();

  // Disables use of the VULKAN feature when creating Contexts. Must be set
  // before calling StartComponent().
  void set_disable_vulkan_for_test() { disable_vulkan_for_test_ = true; }

 private:
  // PendingCastComponent::Delegate implementation.
  void LaunchPendingComponent(PendingCastComponent* pending_component,
                              CastComponent::Params params) override;
  void CancelPendingComponent(PendingCastComponent* pending_component) override;

  // Handlers used to provide parameters for main & isolated Contexts.
  WebContentRunner::WebInstanceConfig GetCommonWebInstanceConfig();
  WebContentRunner::WebInstanceConfig GetMainWebInstanceConfig();
  WebContentRunner::WebInstanceConfig
  GetIsolatedWebInstanceConfigWithFuchsiaDirs(
      std::vector<fuchsia::web::ContentDirectoryProvider> content_directories);
  // TODO(crbug.com/40131115): Remove this once the CastStreamingReceiver
  // Component has been implemented.
  WebContentRunner::WebInstanceConfig
  GetIsolatedWebInstanceConfigForCastStreaming();

  // Returns CreateContextParams for |app_config|. Returns nullopt if there is
  // no need to create an isolated context.
  std::optional<WebContentRunner::WebInstanceConfig>
  GetWebInstanceConfigForAppConfig(
      chromium::cast::ApplicationConfig* app_config);

  // Launches an isolated Context with the given `config` and returns the newly
  // created WebContentRunner.
  WebContentRunner* CreateIsolatedRunner(
      WebContentRunner::WebInstanceConfig config);

  // Called when an isolated component terminates, to allow the Context hosting
  // it to be torn down.
  void OnIsolatedContextEmpty(WebContentRunner* context);

  // Internal implementation of StartComponent(), called after validating the
  // component URL and ensuring that CORS-exempt headers have been fetched.
  void StartComponentInternal(
      const GURL& url,
      std::unique_ptr<base::StartupContext> startup_context,
      fidl::InterfaceRequest<fuchsia::component::runner::ComponentController>
          controller_request);

  // Moves all data persisted by the main Context to a staging directory,
  // which will be deleted the next time the Runner starts up, and configures
  // the Runner to reject new component-launch requests until it is shutdown.
  // Returns false if tha data directory cannot be cleaned-up.
  bool DeletePersistentDataInternal();

  // TODO(crbug.com/40755074): Used to detect when the persisted cache directory
  // was erased. The sentinel file is created at the top-level of the cache
  // directory, so cannot be deleted by the Context, only by the cache being
  // erased.
  void CreatePersistedCacheSentinel();
  bool WasPersistedCacheErased();

  // Passed to WebContentRunners to use to create web_instance Components.
  const raw_ref<WebInstanceHost> web_instance_host_;

  // True if this Runner uses Context(s) with the HEADLESS feature set.
  const bool is_headless_;

  // True if this Runner should create web Contexts with dynamic code generation
  // disabled.
  const bool disable_codegen_;

  // Holds the main fuchsia.web.Context used to host CastComponents.
  // Note that although |main_context_| is actually a WebContentRunner, that is
  // only being used to maintain the Context for the hosted components.
  const std::unique_ptr<WebContentRunner> main_context_;

  // Holds `fuchsia.web.Context`s used to host isolated components.
  base::flat_set<std::unique_ptr<WebContentRunner>, base::UniquePtrComparator>
      isolated_contexts_;

  // Temporarily holds a PendingCastComponent instance, responsible for fetching
  // the parameters required to launch the component, for each call to
  // StartComponent().
  base::flat_set<std::unique_ptr<PendingCastComponent>,
                 base::UniquePtrComparator>
      pending_components_;

  // Used to fetch & cache the list of CORS exempt HTTP headers to configure
  // each web.Context with.
  std::optional<std::vector<std::vector<uint8_t>>> cors_exempt_headers_;
  chromium::cast::CorsExemptHeaderProviderPtr cors_exempt_headers_provider_;
  std::vector<base::OnceClosure> on_have_cors_exempt_headers_;

  // True if Contexts should be created without VULKAN set.
  bool disable_vulkan_for_test_ = false;

  // True if cast runner entered data reset mode. Prevents new components
  // in the main context from being launched. This is set to true once data
  // reset starts and does not switch back to false upon completion.
  bool data_reset_in_progress_ = false;

  // True if the cache sentinel file should exist.
  // TODO(crbug.com/40755074): Remove once an explicit cache flush signal
  // exists.
  bool was_cache_sentinel_created_ = false;
};

#endif  // FUCHSIA_WEB_RUNNERS_CAST_CAST_RUNNER_H_
