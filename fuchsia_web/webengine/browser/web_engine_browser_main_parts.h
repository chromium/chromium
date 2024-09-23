// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_PARTS_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_PARTS_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <memory>
#include <string>
#include <vector>

#include "build/chromecast_buildflags.h"
#include "content/public/browser/browser_main_parts.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/browser/web_engine_browser_context.h"
#include "fuchsia_web/webengine/web_engine_export.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace base {
class FuchsiaIntlProfileWatcher;
}

namespace aura {
class ScreenOzone;
}

namespace content {
class ContentBrowserClient;
}

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
namespace fuchsia_legacymetrics {
class LegacyMetricsClient;
}
#endif

namespace media {
class FuchsiaCdmManager;
}

namespace inspect {
class ComponentInspector;
}

class WebEngineMemoryInspector;

// Implements the fuchsia.web.FrameHost protocol using a ContextImpl with
// incognito browser context.
class FrameHostImpl final : public fuchsia::web::FrameHost {
 public:
  explicit FrameHostImpl(
      inspect::Node inspect_node,
      WebEngineDevToolsController* devtools_controller,
      network::NetworkQualityTracker* network_quality_tracker)
      : context_(
            WebEngineBrowserContext::CreateIncognito(network_quality_tracker),
            std::move(inspect_node),
            devtools_controller) {}
  ~FrameHostImpl() override = default;

  FrameHostImpl(const FrameHostImpl&) = delete;
  FrameHostImpl& operator=(const FrameHostImpl&) = delete;

  // fuchsia.web.FrameHost implementation.
  void CreateFrameWithParams(
      fuchsia::web::CreateFrameParams params,
      fidl::InterfaceRequest<fuchsia::web::Frame> request) override;

  ContextImpl* context_impl_for_test() { return &context_; }

 private:
  ContextImpl context_;
};

class WEB_ENGINE_EXPORT WebEngineBrowserMainParts
    : public content::BrowserMainParts {
 public:
  explicit WebEngineBrowserMainParts(
      content::ContentBrowserClient* browser_client);
  ~WebEngineBrowserMainParts() override;

  WebEngineBrowserMainParts(const WebEngineBrowserMainParts&) = delete;
  WebEngineBrowserMainParts& operator=(const WebEngineBrowserMainParts&) =
      delete;

  std::vector<content::BrowserContext*> browser_contexts() const;
  WebEngineDevToolsController* devtools_controller() const {
    return devtools_controller_.get();
  }

  // content::BrowserMainParts overrides.
  void PostEarlyInitialization() override;
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;

  // Methods used by tests.
  static void SetContextRequestForTest(
      fidl::InterfaceRequest<fuchsia::web::Context> request);

  // Returns the bound ContextImpl instance, or nullptr if there isn't one.
  ContextImpl* context_for_test() const;

  // Returns all FrameHostImpl instances.
  std::vector<FrameHostImpl*> frame_hosts_for_test() const;

 private:
  // Handle fuchsia.web.Context and fuchsia.web.FrameHost connection requests.
  void HandleContextRequest(
      fidl::InterfaceRequest<fuchsia::web::Context> request);
  void HandleFrameHostRequest(
      fidl::InterfaceRequest<fuchsia::web::FrameHost> request);

  // Notified if the system timezone, language, settings change.
  void OnIntlProfileChanged(const fuchsia::intl::Profile& profile);

  // Quits the main loop and gracefully shuts down the instance.
  void BeginGracefulShutdown();

  content::ContentBrowserClient* const browser_client_;

  std::unique_ptr<aura::ScreenOzone> screen_;

  // Used to publish diagnostics including the active Contexts and FrameHosts.
  std::unique_ptr<inspect::ComponentInspector> component_inspector_;
  std::unique_ptr<WebEngineMemoryInspector> memory_inspector_;

  // Browsing contexts for the connected clients. There is at most one
  // fuchsia.web.Context binding, and any number of fuchsia.web.FrameHost
  // bindings.
  fidl::BindingSet<fuchsia::web::Context, std::unique_ptr<ContextImpl>>
      context_bindings_;
  fidl::BindingSet<fuchsia::web::FrameHost, std::unique_ptr<FrameHostImpl>>
      frame_host_bindings_;

  std::unique_ptr<WebEngineDevToolsController> devtools_controller_;

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  std::unique_ptr<fuchsia_legacymetrics::LegacyMetricsClient>
      legacy_metrics_client_;
#endif

  std::unique_ptr<media::FuchsiaCdmManager> cdm_manager_;

  // Used to respond to changes to the system's current locale.
  std::unique_ptr<base::FuchsiaIntlProfileWatcher> intl_profile_watcher_;

  // Used to report networking-related Client Hints.
  std::unique_ptr<network::NetworkQualityTracker> network_quality_tracker_;
  std::unique_ptr<
      network::NetworkQualityTracker::RTTAndThroughputEstimatesObserver>
      network_quality_observer_;

  base::OnceClosure quit_closure_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_PARTS_H_
