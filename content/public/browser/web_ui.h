// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/common/bindings_policy.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace content {

class RenderFrameHost;
class WebContents;
class WebUIController;
class WebUIMessageHandler;

// A WebUI sets up the datasources and message handlers for a given HTML-based
// UI.
class CONTENT_EXPORT WebUI {
 public:
  // An opaque identifier used to identify a WebUI. This can only be compared to
  // kNoWebUI or other WebUI types. See GetWebUIType.
  typedef const void* TypeID;

  // A special WebUI type that signifies that a given page would not use the
  // Web UI system.
  static const TypeID kNoWebUI;

  // Returns JavaScript code that, when executed, calls the function specified
  // by |function_name| with the arguments specified in |arg_list|.
  static std::u16string GetJavascriptCall(
      std::string_view function_name,
      base::span<const base::ValueView> arg_list);
  static std::u16string GetJavascriptCall(std::string_view function_name,
                                          const base::Value::List& arg_list);

  virtual ~WebUI() {}

  virtual WebContents* GetWebContents() = 0;

  virtual WebUIController* GetController() = 0;
  virtual void SetController(std::unique_ptr<WebUIController> controller) = 0;

  // This might return nullptr
  //  1. During construction, until the WebUI is associated with
  //     a RenderFrameHost.
  //  2. During destruction, if the WebUI's destruction causes the
  //     RenderFrameHost to be destroyed (crbug.com/1308391).
  //  3. In unittests where the WebUI is mocked, notably by TestWebUI.
  virtual RenderFrameHost* GetRenderFrameHost() = 0;

  // Returns the device scale factor of the monitor that the renderer is on.
  // Whenever possible, WebUI should push resources with this scale factor to
  // Javascript.
  virtual float GetDeviceScaleFactor() = 0;

  // Gets a custom tab title provided by the Web UI. If there is no title
  // override, the string will be empty which should trigger the default title
  // behavior for the tab.
  virtual const std::u16string& GetOverriddenTitle() = 0;
  virtual void OverrideTitle(const std::u16string& title) = 0;

  // Allows a controller to override the BindingsPolicy that should be enabled
  // for this page.
  virtual BindingsPolicySet GetBindings() = 0;
  virtual void SetBindings(BindingsPolicySet bindings) = 0;

  // Allows a scheme to be requested which is provided by the WebUIController.
  virtual const std::vector<std::string>& GetRequestableSchemes() = 0;
  virtual void AddRequestableScheme(const char* scheme) = 0;

  virtual void AddMessageHandler(
      std::unique_ptr<WebUIMessageHandler> handler) = 0;

  // Used by WebUIMessageHandlers. If the given message is already registered,
  // the call has no effect.
  using MessageCallback =
      base::RepeatingCallback<void(const base::Value::List&)>;
  virtual void RegisterMessageCallback(std::string_view message,
                                       MessageCallback callback) = 0;

  template <typename... Args>
  void RegisterHandlerCallback(
      std::string_view message,
      base::RepeatingCallback<void(Args...)> callback) {
    RegisterMessageCallback(
        message, base::BindRepeating(
                     &Call<std::index_sequence_for<Args...>, Args...>::Impl,
                     callback, message));
  }

  // This is only needed if an embedder overrides handling of a WebUIMessage and
  // then later wants to undo that, or to route it to a different WebUI object.
  virtual void ProcessWebUIMessage(const GURL& source_url,
                                   const std::string& message,
                                   base::Value::List args) = 0;

  // Returns true if this WebUI can currently call JavaScript.
  virtual bool CanCallJavascript() = 0;

  // Calling these functions directly is discouraged. It's generally preferred
  // to call WebUIMessageHandler::CallJavascriptFunction, as that has
  // lifecycle controls to prevent calling JavaScript before the page is ready.
  //
  // Call a Javascript function by sending its name and arguments down to
  // the renderer.  This is asynchronous; there's no way to get the result
  // of the call, and should be thought of more like sending a message to
  // the page.
  //
  // All function names in WebUI must consist of only ASCII characters.
  // There are variants for calls with more arguments.
  void CallJavascriptFunctionUnsafe(std::string_view function_name) {
    CallJavascriptFunctionUnsafe(function_name, {});
  }

  virtual void CallJavascriptFunctionUnsafe(
      std::string_view function_name,
      base::span<const base::ValueView> args) = 0;

  template <typename... Args>
  void CallJavascriptFunctionUnsafe(std::string_view function_name,
                                    const base::ValueView arg1,
                                    const Args&... arg) {
    base::ValueView args[] = {arg1, arg...};
    CallJavascriptFunctionUnsafe(function_name, args);
  }

  // Allows mutable access to this WebUI's message handlers for testing.
  virtual std::vector<std::unique_ptr<WebUIMessageHandler>>*
  GetHandlersForTesting() = 0;

 private:
  template <typename T>
  static T GetValue(const base::Value& value);

  template <typename Is, typename... Args>
  struct Call;

  // Helper to unpack a  base::Value::List  and invoke a callback, passing
  // list[0] as the first argument, list[1] as the second argument, et cetera.
  // Each value in the list will be coerced to the type of the corresponding
  // function parameter, CHECK()ing if the conversion is not possible or if the
  // number of arguments is wrong.
  template <size_t... Is, typename... Args>
  struct Call<std::index_sequence<Is...>, Args...> {
    static void Impl(base::RepeatingCallback<void(Args...)> callback,
                     std::string_view message,
                     const base::Value::List& list) {
      CHECK_EQ(list.size(), sizeof...(Args)) << message;
      callback.Run(GetValue<Args>(list[Is])...);
    }
  };
};

template <>
inline bool WebUI::GetValue<bool>(const base::Value& value) {
  return value.GetBool();
}

template <>
inline int WebUI::GetValue<int>(const base::Value& value) {
  return value.GetInt();
}

template <>
inline const std::string& WebUI::GetValue<const std::string&>(
    const base::Value& value) {
  return value.GetString();
}

template <>
inline const base::Value::Dict& WebUI::GetValue<const base::Value::Dict&>(
    const base::Value& value) {
  return value.GetDict();
}

template <>
inline const base::Value::List& WebUI::GetValue<const base::Value::List&>(
    const base::Value& value) {
  return value.GetList();
}

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_H_
