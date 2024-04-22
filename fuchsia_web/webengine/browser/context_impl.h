// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_CONTEXT_IMPL_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_CONTEXT_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/inspect/cpp/vmo/types.h>
#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "build/chromecast_buildflags.h"
#include "fuchsia_web/webengine/browser/cookie_manager_impl.h"
#include "fuchsia_web/webengine/web_engine_export.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

class FrameImpl;
class WebEngineDevToolsController;

// Implementation of Context from fuchsia.web.
// Owns a BrowserContext instance and uses it to create new WebContents/Frames.
// All created Frames are owned by this object.
class WEB_ENGINE_EXPORT ContextImpl final : public fuchsia::web::Context {
 public:
  // |devtools_controller| must outlive ContextImpl.
  // Diagnostics about the context will be placed in |inspect_node|.
  ContextImpl(std::unique_ptr<content::BrowserContext> browser_context,
              inspect::Node inspect_node,
              WebEngineDevToolsController* devtools_controller);

  // Tears down the Context, destroying any active Frames in the process.
  ~ContextImpl() override;

  ContextImpl(const ContextImpl&) = delete;
  ContextImpl& operator=(const ContextImpl&) = delete;

  // Removes and destroys the specified |frame|.
  void DestroyFrame(FrameImpl* frame);

  // Returns |true| if JS injection was enabled for this Context.
  bool IsJavaScriptInjectionAllowed();

  // Creates a Frame with |params| for the |web_contents| and binds it to
  // |frame_request|. The Frame will self-delete when |frame_request|
  // disconnects. |params| must be clonable as required by FrameImpl.
  FrameImpl* CreateFrameForWebContents(
      std::unique_ptr<content::WebContents> web_contents,
      fuchsia::web::CreateFrameParams params,
      fidl::InterfaceRequest<fuchsia::web::Frame> frame_request);

  // Returns the DevTools controller for this Context.
  WebEngineDevToolsController* devtools_controller() const {
    return devtools_controller_;
  }

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  // Controls whether the CastStreaming receiver is available in this instance.
  // At most one ContextImpl per-process may have CastStreaming enabled.
  void SetCastStreamingEnabled();
  bool has_cast_streaming_enabled() const { return cast_streaming_enabled_; }
#endif

  // fuchsia::web::Context implementation.
  void CreateFrame(fidl::InterfaceRequest<fuchsia::web::Frame> frame) override;
  void CreateFrameWithParams(
      fuchsia::web::CreateFrameParams params,
      fidl::InterfaceRequest<fuchsia::web::Frame> frame) override;
  void GetCookieManager(
      fidl::InterfaceRequest<fuchsia::web::CookieManager> manager) override;
  void GetRemoteDebuggingPort(GetRemoteDebuggingPortCallback callback) override;

  // Gets the underlying FrameImpl service object associated with a connected
  // |frame_ptr| client.
  FrameImpl* GetFrameImplForTest(fuchsia::web::FramePtr* frame_ptr) const;

  content::BrowserContext* browser_context() const {
    return browser_context_.get();
  }

 private:
  // Returns the NetworkContext from the default StoragePartition.
  network::mojom::NetworkContext* GetNetworkContext();

  // Reference to the browser implementation for this Context.
  std::unique_ptr<content::BrowserContext> const browser_context_;

  // Reference to the class managing the DevTools remote debugging service.
  WebEngineDevToolsController* const devtools_controller_;

  // Inspect node & properties for this browsing context.
  inspect::Node inspect_node_;

  // CookieManager API implementation for this Context.
  CookieManagerImpl cookie_manager_;
  fidl::BindingSet<fuchsia::web::CookieManager> cookie_manager_bindings_;

  // TODO(crbug.com/40597158): Make this false by default, and allow it to be
  // initialized at Context creation time.
  bool allow_javascript_injection_ = true;

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  // True if this instance should allows Frames to use CastStreaming.
  bool cast_streaming_enabled_ = false;
#endif

  // Tracks all active FrameImpl instances, so that we can request their
  // destruction when this ContextImpl is destroyed.
  std::set<std::unique_ptr<FrameImpl>, base::UniquePtrComparator> frames_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_CONTEXT_IMPL_H_
