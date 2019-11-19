// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_MESSAGE_HANDLER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_MESSAGE_HANDLER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_ui.h"

class WebUIBrowserTest;
class MojoWebUIBrowserTest;
class CertificateHandlerTest;

namespace base {
class ListValue;
}

namespace content {

class TestWebUI;
class WebUI;
class WebUIImpl;

// Messages sent from the DOM are forwarded via the WebUI to handler
// classes. These objects are owned by WebUI and destroyed when the
// host is destroyed.
class CONTENT_EXPORT WebUIMessageHandler {
 public:
  WebUIMessageHandler() : javascript_allowed_(false), web_ui_(nullptr) {}
  virtual ~WebUIMessageHandler() {}

  // Call this when a page should not receive JavaScript messages.
  void DisallowJavascript();

  // Called from tests to toggle JavaScript to catch bugs. If AllowJavascript()
  // is needed from production code, just publicize AllowJavascript() instead.
  void AllowJavascriptForTesting();

  bool IsJavascriptAllowed() const;

 protected:
  FRIEND_TEST_ALL_PREFIXES(WebUIMessageHandlerTest, ExtractIntegerValue);
  FRIEND_TEST_ALL_PREFIXES(WebUIMessageHandlerTest, ExtractDoubleValue);
  FRIEND_TEST_ALL_PREFIXES(WebUIMessageHandlerTest, ExtractStringValue);

  // This method must be called once the handler's corresponding JavaScript
  // component is initialized. In practice, it should be called from a WebUI
  // message handler similar to: 'initializeFooPage' or 'getInitialState'.
  //
  // There should be ideally one or two calls to this per handler, as JavaScript
  // components should have a specific message that signals that it's initalized
  // and ready to receive events from the C++ handler.
  //
  // This should never be called from a function that is not a message handler.
  // This should never be called from a C++ callback used as a reply for a
  // posted task or asynchronous operation.
  void AllowJavascript();

  // Helper methods:

  // Extract an integer value from a list Value.
  static bool ExtractIntegerValue(const base::ListValue* value, int* out_int);

  // Extract a floating point (double) value from a list Value.
  static bool ExtractDoubleValue(const base::ListValue* value,
                                 double* out_value);

  // Extract a string value from a list Value.
  static base::string16 ExtractStringValue(const base::ListValue* value);

  // This is where subclasses specify which messages they'd like to handle and
  // perform any additional initialization.. At this point web_ui() will return
  // the associated WebUI object.
  virtual void RegisterMessages() = 0;

  // Will be called whenever JavaScript from this handler becomes allowed from
  // the disallowed state. Subclasses should override this method to register
  // observers that push JavaScript calls to the page.
  virtual void OnJavascriptAllowed() {}

  // Will be called whenever JavaScript from this handler becomes disallowed
  // from the allowed state. This will never be called before
  // OnJavascriptAllowed has been called. Subclasses should override this method
  // to deregister or disabled observers that push JavaScript calls to the page.
  virtual void OnJavascriptDisallowed() {}

  // Helper method for responding to Javascript requests initiated with
  // cr.sendWithPromise() (defined in cr.js) for the case where the returned
  // promise should be resolved (request succeeded).
  void ResolveJavascriptCallback(const base::Value& callback_id,
                                 const base::Value& response);

  // Helper method for responding to Javascript requests initiated with
  // cr.sendWithPromise() (defined in cr.js), for the case where the returned
  // promise should be rejected (request failed).
  void RejectJavascriptCallback(const base::Value& callback_id,
                                const base::Value& response);

  // Helper method for notifying Javascript listeners added with
  // cr.addWebUIListener() (defined in cr.js).
  template <typename... Values>
  void FireWebUIListener(const std::string& event_name,
                         const Values&... values) {
    // cr.webUIListenerCallback is a global JS function exposed from cr.js.
    CallJavascriptFunction("cr.webUIListenerCallback", base::Value(event_name),
                           values...);
  }

  // Call a Javascript function by sending its name and arguments down to
  // the renderer.  This is asynchronous; there's no way to get the result
  // of the call, and should be thought of more like sending a message to
  // the page.
  // All function names in WebUI must consist of only ASCII characters.
  // These functions will crash if JavaScript is not currently allowed.
  template <typename... Values>
  void CallJavascriptFunction(const std::string& function_name,
                              const Values&... values) {
    CHECK(IsJavascriptAllowed()) << "Cannot CallJavascriptFunction before "
                                    "explicitly allowing JavaScript.";

    // The CHECK above makes this call safe.
    web_ui()->CallJavascriptFunctionUnsafe(function_name, values...);
  }

  // Returns the attached WebUI for this handler.
  WebUI* web_ui() const { return web_ui_; }

  // Sets the attached WebUI - exposed to subclasses for testing purposes.
  void set_web_ui(WebUI* web_ui) { web_ui_ = web_ui; }

 private:
  // Provide external classes access to web_ui(), set_web_ui(), and
  // RenderViewReused.
  friend class TestWebUI;
  friend class WebUIImpl;
  friend class ::WebUIBrowserTest;
  friend class ::MojoWebUIBrowserTest;
  friend class ::CertificateHandlerTest;

  // TODO(dbeam): disallow JavaScript when a renderer process crashes.
  // http://crbug.com/610450

  // True if the page is for JavaScript calls from this handler.
  bool javascript_allowed_;

  WebUI* web_ui_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_MESSAGE_HANDLER_H_
