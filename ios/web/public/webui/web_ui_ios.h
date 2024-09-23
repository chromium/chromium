// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_H_
#define IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"

class GURL;

namespace web {

class WebState;
class WebUIIOSController;
class WebUIIOSMessageHandler;

// A WebUIIOS sets up the datasources and message handlers for a given
// HTML-based UI.
class WebUIIOS {
 public:
  // Returns JavaScript code that, when executed, calls the function specified
  // by `function_name` with the arguments specified in `arg_list`.
  static std::u16string GetJavascriptCall(
      std::string_view function_name,
      base::span<const base::ValueView> arg_list);

  virtual ~WebUIIOS() {}

  virtual web::WebState* GetWebState() const = 0;

  virtual WebUIIOSController* GetController() const = 0;
  virtual void SetController(
      std::unique_ptr<WebUIIOSController> controller) = 0;

  // Takes ownership of `handler`, which will be destroyed when the WebUIIOS is.
  virtual void AddMessageHandler(
      std::unique_ptr<WebUIIOSMessageHandler> handler) = 0;

  // Used by WebUIMessageHandlers. If the given message is already registered,
  // the call has no effect.
  using MessageCallback =
      base::RepeatingCallback<void(const base::Value::List&)>;
  virtual void RegisterMessageCallback(std::string_view message,
                                       MessageCallback callback) = 0;

  // This is only needed if an embedder overrides handling of a WebUIIOSMessage
  // and then later wants to undo that, or to route it to a different WebUIIOS
  // object.
  virtual void ProcessWebUIIOSMessage(const GURL& source_url,
                                      std::string_view message,
                                      const base::Value::List& args) = 0;

  // Call a Javascript function.  This is asynchronous; there's no way to get
  // the result of the call, and should be thought of more like sending a
  // message to the page.  All function names in WebUI must consist of only
  // ASCII characters.
  virtual void CallJavascriptFunction(
      std::string_view function_name,
      base::span<const base::ValueView> args) = 0;

  // Helper method for responding to Javascript requests initiated with
  // cr.sendWithPromise() (defined in cr.js) for the case where the returned
  // promise should be resolved (request succeeded).
  virtual void ResolveJavascriptCallback(const base::ValueView callback_id,
                                         const base::ValueView response) = 0;

  // Helper method for responding to Javascript requests initiated with
  // cr.sendWithPromise() (defined in cr.js), for the case where the returned
  // promise should be rejected (request failed).
  virtual void RejectJavascriptCallback(const base::ValueView callback_id,
                                        const base::ValueView response) = 0;

  // Helper method for notifying Javascript listeners added with
  // cr.addWebUIListener() (defined in cr.js).
  virtual void FireWebUIListenerSpan(
      base::span<const base::ValueView> values) = 0;

  // Helper method for notifying Javascript listeners added with
  // cr.addWebUIListener() (defined in cr.js).
  template <typename... Arg>
  void FireWebUIListener(std::string_view event_name, const Arg&... arg) {
    base::Value callback_arg(event_name);
    base::ValueView args[] = {callback_arg, arg...};
    FireWebUIListenerSpan(args);
  }
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_H_
