// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_H_

#include <ostream>
#include <string>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/page.h"
#include "content/public/browser/per_web_ui_browser_interface_broker.h"

class GURL;

namespace content {

class RenderFrameHost;
class WebUI;
class WebUIBrowserInterfaceBrokerRegistry;

// A WebUI page is controlled by the embedder's WebUIController object. It
// manages the data source and message handlers.
class CONTENT_EXPORT WebUIController {
 public:
  // An opaque identifier used to identify a WebUIController's concrete type.
  // This is used for safe downcasting.
  typedef const void* Type;

  explicit WebUIController(WebUI* web_ui);
  virtual ~WebUIController();

  // Allows the controller to override handling all messages from the page.
  // Return true if the message handling was overridden.
  virtual bool OverrideHandleWebUIMessage(const GURL& source_url,
                                          const std::string& message,
                                          const base::Value::List& args);

  // Called when a WebUI RenderFrame is created.  This is *not* called for every
  // page load because in some cases a RenderFrame will be reused, for example
  // when reloading or navigating to a same-site URL.
  // This is deliberately named to differentiate from
  // WebContentsObserver::RenderFrameCreated, as some classes may override both.
  virtual void WebUIRenderFrameCreated(RenderFrameHost* render_frame_host) {}

  // Called when the WebUI's primary page changes. WebUIControllers should reset
  // its state if necessary.
  virtual void WebUIPrimaryPageChanged(Page& page) {}

  // Called when a WebUI page load is about to be committed, even if RenderFrame
  // is reused. This sets up MojoJS interface broker.
  void WebUIReadyToCommitNavigation(RenderFrameHost* render_frame_host);

  WebUI* web_ui() const { return web_ui_; }

  // Performs a safe downcast to a WebUIController subclass.
  template <typename T>
  T* GetAs() {
    CHECK(GetType())
        << "WebUIController::GetAs() called on subclass which is missing "
           "WEB_UI_CONTROLLER_TYPE_DECL().";

    return GetType() == &T::kWebUIControllerType ? static_cast<T*>(this)
                                                 : nullptr;
  }

  // Controls whether the engineering team receives JavaScript error reports for
  // this WebUI. For example, WebUIs may report JavaScript errors and unhandled
  // exceptions to an error reporting service if this function isn't called.
  //
  // WebUIs may want to override this function if they are reporting errors via
  // other channels and don't want duplicates. For instance, a WebUI which uses
  // crashReportPrivate to report JS errors might override this function to
  // return to false in order to avoid duplicate reports. WebUIs might also
  // override this function to return false to avoid noise if the engineering
  // team doesn't expect to fix reported errors; for instance, a low-usage
  // debugging page might turn off error reports if the owners feel any reported
  // bugs would be too low priority to bother with.
  virtual bool IsJavascriptErrorReportingEnabled();

  // TODO(calamity): Make this abstract once all subclasses implement GetType().
  virtual Type GetType();

  PerWebUIBrowserInterfaceBroker* broker_for_testing() { return broker_.get(); }

 private:
  raw_ptr<WebUI> web_ui_;

  // The interface broker that handles Mojo.bindInterface requests from the
  // renderer.
  std::unique_ptr<PerWebUIBrowserInterfaceBroker> broker_;
};

// This macro declares a static variable inside the class that inherits from
// WebUIController. The address of the static variable is used as the unique
// Type for the subclass.
#define WEB_UI_CONTROLLER_TYPE_DECL()        \
  static const int kWebUIControllerType = 0; \
  Type GetType() final;                      \
  friend class content::WebUIController;     \
  friend class content::WebUIBrowserInterfaceBrokerRegistry

// This macro instantiates the static variable declared by the previous macro.
// It must live in a .cc file to ensure that there is only one instantiation
// of the static variable.
#define WEB_UI_CONTROLLER_TYPE_IMPL(T) \
  const int T::kWebUIControllerType;   \
  content::WebUIController::Type T::GetType() { return &kWebUIControllerType; }

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_H_
