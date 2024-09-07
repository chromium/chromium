// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_WEB_UI_H_
#define CONTENT_PUBLIC_TEST_TEST_WEB_UI_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

namespace content {

// Test instance of WebUI that tracks the data passed to
// CallJavascriptFunctionUnsafe().
class TestWebUI : public WebUI {
 public:
  TestWebUI();

  TestWebUI(const TestWebUI&) = delete;
  TestWebUI& operator=(const TestWebUI&) = delete;

  ~TestWebUI() override;

  void ClearTrackedCalls();
  void HandleReceivedMessage(const std::string& handler_name,
                             const base::Value::List& args);

  void set_web_contents(WebContents* web_contents) {
    web_contents_ = web_contents;
  }

  void set_render_frame_host(RenderFrameHost* render_frame_host) {
    render_frame_host_ = render_frame_host;
  }

  // WebUI overrides.
  WebContents* GetWebContents() override;
  WebUIController* GetController() override;
  RenderFrameHost* GetRenderFrameHost() override;
  void SetController(std::unique_ptr<WebUIController> controller) override;
  float GetDeviceScaleFactor() override;
  const std::u16string& GetOverriddenTitle() override;
  void OverrideTitle(const std::u16string& title) override {}
  BindingsPolicySet GetBindings() override;
  void SetBindings(BindingsPolicySet bindings) override;
  const std::vector<std::string>& GetRequestableSchemes() override;
  void AddRequestableScheme(const char* scheme) override;
  void AddMessageHandler(std::unique_ptr<WebUIMessageHandler> handler) override;
  void RegisterMessageCallback(std::string_view message,
                               MessageCallback callback) override;
  void ProcessWebUIMessage(const GURL& source_url,
                           const std::string& message,
                           base::Value::List args) override;
  bool CanCallJavascript() override;
  void CallJavascriptFunctionUnsafe(
      std::string_view function_name,
      base::span<const base::ValueView> args) override;
  std::vector<std::unique_ptr<WebUIMessageHandler>>* GetHandlersForTesting()
      override;

  class CallData {
   public:
    explicit CallData(std::string_view function_name);
    ~CallData();

    void AppendArgument(base::Value arg);

    const std::string& function_name() const { return function_name_; }
    const base::Value* arg_nth(size_t index) const {
      return args_.size() > index ? &args_[index] : nullptr;
    }
    const base::Value* arg1() const { return arg_nth(0); }
    const base::Value* arg2() const { return arg_nth(1); }
    const base::Value* arg3() const { return arg_nth(2); }
    const base::Value* arg4() const { return arg_nth(3); }

    const base::Value::List& args() const { return args_; }

   private:
    std::string function_name_;
    base::Value::List args_;
  };

  const std::vector<std::unique_ptr<CallData>>& call_data() const {
    return call_data_;
  }

  // An observer that will be notified of javascript calls.
  class JavascriptCallObserver : public base::CheckedObserver {
   public:
    virtual void OnJavascriptFunctionCalled(const CallData& call_data) = 0;
  };

  void AddJavascriptCallObserver(JavascriptCallObserver* obs) {
    javascript_call_observers_.AddObserver(obs);
  }

  void RemoveJavascriptCallObserver(JavascriptCallObserver* obs) {
    javascript_call_observers_.RemoveObserver(obs);
  }

 private:
  void OnJavascriptCall(const CallData& call_data);

  base::flat_map<std::string, std::vector<MessageCallback>> message_callbacks_;
  std::vector<std::unique_ptr<CallData>> call_data_;
  std::vector<std::unique_ptr<WebUIMessageHandler>> handlers_;
  BindingsPolicySet bindings_;
  std::u16string temp_string_;
  raw_ptr<WebContents, AcrossTasksDanglingUntriaged> web_contents_ = nullptr;
  raw_ptr<RenderFrameHost, AcrossTasksDanglingUntriaged> render_frame_host_ =
      nullptr;
  std::unique_ptr<WebUIController> controller_;

  // Observers to be notified on all javascript calls.
  base::ObserverList<JavascriptCallObserver> javascript_call_observers_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_WEB_UI_H_
