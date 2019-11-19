// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_CONTEXT_IMPL_H_
#define FUCHSIA_ENGINE_BROWSER_CONTEXT_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "fuchsia/engine/browser/cookie_manager_impl.h"
#include "fuchsia/engine/web_engine_export.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

class FrameImpl;
class WebEngineDevToolsController;
class WebEngineMemoryPressureEvaluator;

// Implementation of Context from fuchsia.web.
// Owns a BrowserContext instance and uses it to create new WebContents/Frames.
// All created Frames are owned by this object.
class WEB_ENGINE_EXPORT ContextImpl : public fuchsia::web::Context {
 public:
  // |browser_context| and |devtools_controller| must outlive ContextImpl.
  ContextImpl(content::BrowserContext* browser_context,
              WebEngineDevToolsController* devtools_controller);

  // Tears down the Context, destroying any active Frames in the process.
  ~ContextImpl() final;

  // Removes and destroys the specified |frame|.
  void DestroyFrame(FrameImpl* frame);

  // Returns |true| if JS injection was enabled for this Context.
  bool IsJavaScriptInjectionAllowed();

  // Registers a Frame originating from web content (i.e. a popup).
  fidl::InterfaceHandle<fuchsia::web::Frame> CreateFrameForPopupWebContents(
      std::unique_ptr<content::WebContents> web_contents);

  // Returns the DevTools controller for this Context.
  WebEngineDevToolsController* devtools_controller() const {
    return devtools_controller_;
  }

  // fuchsia::web::Context implementation.
  void CreateFrame(fidl::InterfaceRequest<fuchsia::web::Frame> frame) final;
  void CreateFrameWithParams(
      fuchsia::web::CreateFrameParams params,
      fidl::InterfaceRequest<fuchsia::web::Frame> frame) final;
  void GetCookieManager(
      fidl::InterfaceRequest<fuchsia::web::CookieManager> manager) final;
  void GetRemoteDebuggingPort(GetRemoteDebuggingPortCallback callback) final;

  // Gets the underlying FrameImpl service object associated with a connected
  // |frame_ptr| client.
  FrameImpl* GetFrameImplForTest(fuchsia::web::FramePtr* frame_ptr) const;

  content::BrowserContext* browser_context_for_test() const {
    return browser_context_;
  }

 private:
  // Reference to the browser implementation for this Context.
  content::BrowserContext* const browser_context_;

  // Reference to the class managing the DevTools remote debugging service.
  WebEngineDevToolsController* const devtools_controller_;

  // CookieManager API implementation for this Context.
  CookieManagerImpl cookie_manager_;
  fidl::BindingSet<fuchsia::web::CookieManager> cookie_manager_bindings_;

  // TODO(crbug.com/893236): Make this false by default, and allow it to be
  // initialized at Context creation time.
  bool allow_javascript_injection_ = true;

  // Tracks all active FrameImpl instances, so that we can request their
  // destruction when this ContextImpl is destroyed.
  std::set<std::unique_ptr<FrameImpl>, base::UniquePtrComparator> frames_;

  // Synthesizes MemoryPressureLevel values & notifications to manage the
  // Context process' memory footprint.
  std::unique_ptr<WebEngineMemoryPressureEvaluator> memory_pressure_evaluator_;

  DISALLOW_COPY_AND_ASSIGN(ContextImpl);
};

#endif  // FUCHSIA_ENGINE_BROWSER_CONTEXT_IMPL_H_
