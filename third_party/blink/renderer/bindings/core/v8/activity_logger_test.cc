// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "v8/include/v8.h"

namespace blink {

using blink::frame_test_helpers::WebViewHelper;
using blink::frame_test_helpers::PumpPendingRequestsForFrameToLoad;

class TestActivityLogger : public V8DOMActivityLogger {
 public:
  ~TestActivityLogger() override = default;

  void LogGetter(const String& api_name) override {
    logged_activities_.push_back(api_name);
  }

  void LogSetter(const String& api_name,
                 const v8::Local<v8::Value>& new_value) override {
    logged_activities_.push_back(
        api_name + " | " + ToCoreStringWithUndefinedOrNullCheck(new_value));
  }

  void LogMethod(const String& api_name,
                 int argc,
                 const v8::Local<v8::Value>* argv) override {
    String activity_string = api_name;
    for (int i = 0; i < argc; i++) {
      activity_string = activity_string + " | " +
                        ToCoreStringWithUndefinedOrNullCheck(argv[i]);
    }
    logged_activities_.push_back(activity_string);
  }

  void LogEvent(const String& event_name,
                int argc,
                const String* argv) override {
    String activity_string = event_name;
    for (int i = 0; i < argc; i++) {
      activity_string = activity_string + " | " + argv[i];
    }
    logged_activities_.push_back(activity_string);
  }

  void clear() { logged_activities_.clear(); }
  bool VerifyActivities(const Vector<String>& expected) const {
    EXPECT_EQ(expected.size(), logged_activities_.size());
    for (wtf_size_t i = 0;
         i < std::min(expected.size(), logged_activities_.size()); ++i) {
      EXPECT_EQ(expected[i], logged_activities_[i]);
    }
    return logged_activities_ == expected;
  }

 private:
  Vector<String> logged_activities_;
};

class ActivityLoggerTest : public testing::Test {
 protected:
  ActivityLoggerTest() {
    activity_logger_ = new TestActivityLogger();
    V8DOMActivityLogger::SetActivityLogger(kIsolatedWorldId, String(),
                                           base::WrapUnique(activity_logger_));
    web_view_helper_.Initialize();
    script_controller_ = &web_view_helper_.GetWebView()
                              ->MainFrameImpl()
                              ->GetFrame()
                              ->GetScriptController();
    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(), "about:blank");
  }

  ~ActivityLoggerTest() override {
    WebCache::Clear();
    V8GCController::CollectAllGarbageForTesting(v8::Isolate::GetCurrent());
  }

  void ExecuteScriptInMainWorld(const String& script) const {
    v8::HandleScope scope(v8::Isolate::GetCurrent());
    script_controller_->ExecuteScriptInMainWorld(script);
    PumpPendingRequestsForFrameToLoad(web_view_helper_.LocalMainFrame());
  }

  void ExecuteScriptInIsolatedWorld(const String& script) const {
    v8::HandleScope scope(v8::Isolate::GetCurrent());
    script_controller_->ExecuteScriptInIsolatedWorld(
        kIsolatedWorldId, ScriptSourceCode(script), KURL(),
        SanitizeScriptErrors::kSanitize);
    PumpPendingRequestsForFrameToLoad(web_view_helper_.LocalMainFrame());
  }

  bool VerifyActivities(const String& activities) {
    Vector<String> activity_vector;
    activities.Split("\n", activity_vector);
    return activity_logger_->VerifyActivities(activity_vector);
  }

 private:
  static const int kIsolatedWorldId = 1;

  WebViewHelper web_view_helper_;
  Persistent<ScriptController> script_controller_;
  // TestActivityLogger is owned by a static table within V8DOMActivityLogger
  // and should be alive as long as not overwritten.
  TestActivityLogger* activity_logger_;
};

TEST_F(ActivityLoggerTest, EventHandler) {
  const char* code =
      "document.body.innerHTML = '<a onclick=\\\'do()\\\'>test</a>';"
      "document.body.onchange = function(){};"
      "document.body.setAttribute('onfocus', 'fnc()');"
      "document.body.addEventListener('onload', function(){});";
  const char* expected_activities =
      "blinkAddEventListener | A | click\n"
      "blinkAddElement | a | \n"
      "blinkAddEventListener | BODY | change\n"
      "blinkAddEventListener | DOMWindow | focus\n"
      "blinkAddEventListener | BODY | onload";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, ScriptElement) {
  const char* code =
      "document.body.innerHTML = '<script "
      "src=\\\'data:text/javascript;charset=utf-8,\\\'></script>';"
      "document.body.innerHTML = '<script>console.log(\\\'test\\\')</script>';"
      "var script = document.createElement('script');"
      "document.body.appendChild(script);"
      "script = document.createElement('script');"
      "script.src = 'data:text/javascript;charset=utf-8,';"
      "document.body.appendChild(script);"
      "document.write('<body><script "
      "src=\\\'data:text/javascript;charset=utf-8,\\\'></script></body>');"
      "document.close();";
  const char* expected_activities =
      "blinkAddElement | script | data:text/javascript;charset=utf-8,\n"
      "blinkAddElement | script | \n"
      "blinkAddElement | script | \n"
      "blinkAddElement | script | data:text/javascript;charset=utf-8,\n"
      "blinkRequestResource | Script | data:text/javascript;charset=utf-8,\n"
      "blinkAddElement | script | data:text/javascript;charset=utf-8,\n"
      "blinkRequestResource | Script | data:text/javascript;charset=utf-8,";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, IFrameElement) {
  const char* code =
      "document.body.innerHTML = '<iframe "
      "src=\\\'data:text/html;charset=utf-8,\\\'></iframe>';"
      "document.body.innerHTML = '<iframe></iframe>';"
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);"
      "iframe = document.createElement('iframe');"
      "iframe.src = 'data:text/html;charset=utf-8,';"
      "document.body.appendChild(iframe);"
      "document.write('<body><iframe "
      "src=\\\'data:text/html;charset=utf-8,\\\'></iframe></body>');"
      "document.close();";
  const char* expected_activities =
      "blinkAddElement | iframe | data:text/html;charset=utf-8,\n"
      "blinkRequestResource | Main resource | data:text/html;charset=utf-8,\n"
      "blinkAddElement | iframe | \n"
      "blinkAddElement | iframe | \n"
      "blinkAddElement | iframe | data:text/html;charset=utf-8,\n"
      "blinkRequestResource | Main resource | data:text/html;charset=utf-8,\n"
      "blinkAddElement | iframe | data:text/html;charset=utf-8,\n"
      "blinkRequestResource | Main resource | data:text/html;charset=utf-8,";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, AnchorElement) {
  const char* code =
      "document.body.innerHTML = '<a "
      "href=\\\'data:text/css;charset=utf-8,\\\'></a>';"
      "document.body.innerHTML = '<a></a>';"
      "var a = document.createElement('a');"
      "document.body.appendChild(a);"
      "a = document.createElement('a');"
      "a.href = 'data:text/css;charset=utf-8,';"
      "document.body.appendChild(a);"
      "document.write('<body><a "
      "href=\\\'data:text/css;charset=utf-8,\\\'></a></body>');"
      "document.close();";
  const char* expected_activities =
      "blinkAddElement | a | data:text/css;charset=utf-8,\n"
      "blinkAddElement | a | \n"
      "blinkAddElement | a | \n"
      "blinkAddElement | a | data:text/css;charset=utf-8,\n"
      "blinkAddElement | a | data:text/css;charset=utf-8,";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, LinkElement) {
  const char* code =
      "document.body.innerHTML = '<link rel=\\\'stylesheet\\\' "
      "href=\\\'data:text/css;charset=utf-8,\\\'></link>';"
      "document.body.innerHTML = '<link></link>';"
      "var link = document.createElement('link');"
      "document.body.appendChild(link);"
      "link = document.createElement('link');"
      "link.rel = 'stylesheet';"
      "link.href = 'data:text/css;charset=utf-8,';"
      "document.body.appendChild(link);"
      "document.write('<body><link rel=\\\'stylesheet\\\' "
      "href=\\\'data:text/css;charset=utf-8,\\\'></link></body>');"
      "document.close();";
  const char* expected_activities =
      "blinkAddElement | link | stylesheet | data:text/css;charset=utf-8,\n"
      "blinkRequestResource | CSS stylesheet | data:text/css;charset=utf-8,\n"
      "blinkAddElement | link |  | \n"
      "blinkAddElement | link |  | \n"
      "blinkAddElement | link | stylesheet | data:text/css;charset=utf-8,\n"
      "blinkRequestResource | CSS stylesheet | data:text/css;charset=utf-8,\n"
      "blinkAddElement | link | stylesheet | data:text/css;charset=utf-8,\n"
      "blinkRequestResource | CSS stylesheet | data:text/css;charset=utf-8,";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, InputElement) {
  const char* code =
      "document.body.innerHTML = '<input type=\\\'submit\\\' "
      "formaction=\\\'data:text/html;charset=utf-8,\\\'></input>';"
      "document.body.innerHTML = '<input></input>';"
      "var input = document.createElement('input');"
      "document.body.appendChild(input);"
      "input = document.createElement('input');"
      "input.type = 'submit';"
      "input.formAction = 'data:text/html;charset=utf-8,';"
      "document.body.appendChild(input);"
      "document.write('<body><input type=\\\'submit\\\' "
      "formaction=\\\'data:text/html;charset=utf-8,\\\'></input></body>');"
      "document.close();";
  const char* expected_activities =
      "blinkAddElement | input | submit | data:text/html;charset=utf-8,\n"
      "blinkAddElement | input |  | \n"
      "blinkAddElement | input |  | \n"
      "blinkAddElement | input | submit | data:text/html;charset=utf-8,\n"
      "blinkAddElement | input | submit | data:text/html;charset=utf-8,";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, ButtonElement) {
  const char* code =
      "document.body.innerHTML = '<button type=\\\'submit\\\' "
      "formmethod=\\\'post\\\' "
      "formaction=\\\'data:text/html;charset=utf-8,\\\'></input>';"
      "document.body.innerHTML = '<button></button>';"
      "var button = document.createElement('button');"
      "document.body.appendChild(button);"
      "button = document.createElement('button');"
      "button.type = 'submit';"
      "button.formMethod = 'post';"
      "button.formAction = 'data:text/html;charset=utf-8,';"
      "document.body.appendChild(button);"
      "document.write('<body><button type=\\\'submit\\\' "
      "formmethod=\\\'post\\\' "
      "formaction=\\\'data:text/html;charset=utf-8,\\\'></button></body>');"
      "document.close();";
  const char* expected_activities =
      "blinkAddElement | button | submit | post | "
      "data:text/html;charset=utf-8,\n"
      "blinkAddElement | button |  |  | \n"
      "blinkAddElement | button |  |  | \n"
      "blinkAddElement | button | submit | post | "
      "data:text/html;charset=utf-8,\n"
      "blinkAddElement | button | submit | post | "
      "data:text/html;charset=utf-8,";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, FormElement) {
  const char* code =
      "document.body.innerHTML = '<form method=\\\'post\\\' "
      "action=\\\'data:text/html;charset=utf-8,\\\'></form>';"
      "document.body.innerHTML = '<form></form>';"
      "var form = document.createElement('form');"
      "document.body.appendChild(form);"
      "form = document.createElement('form');"
      "form.method = 'post';"
      "form.action = 'data:text/html;charset=utf-8,';"
      "document.body.appendChild(form);"
      "document.write('<body><form method=\\\'post\\\' "
      "action=\\\'data:text/html;charset=utf-8,\\\'></form></body>');"
      "document.close();";
  const char* expected_activities =
      "blinkAddElement | form | post | data:text/html;charset=utf-8,\n"
      "blinkAddElement | form |  | \n"
      "blinkAddElement | form |  | \n"
      "blinkAddElement | form | post | data:text/html;charset=utf-8,\n"
      "blinkAddElement | form | post | data:text/html;charset=utf-8,";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, ScriptSrcAttribute) {
  const char* code =
      "document.open();"
      "document.write('<script "
      "src=\\\'data:text/javascript;charset=utf-8,A\\\'></script>');"
      "document.close();"
      "var script = document.getElementsByTagName('script')[0];"
      "script.src = 'data:text/javascript;charset=utf-8,B';"
      "script.setAttribute('src', 'data:text/javascript;charset=utf-8,C');"
      "script.setAttributeNS('', 'src', "
      "'data:text/javascript;charset=utf-8,D');"
      "var attr = document.createAttribute('src');"
      "attr.value = 'data:text/javascript;charset=utf-8,E';"
      "script.setAttributeNode(attr);";
  const char* expected_activities =
      "blinkAddElement | script | data:text/javascript;charset=utf-8,A\n"
      "blinkRequestResource | Script | data:text/javascript;charset=utf-8,A\n"
      "blinkSetAttribute | script | src | data:text/javascript;charset=utf-8,A "
      "| data:text/javascript;charset=utf-8,B\n"
      "blinkSetAttribute | script | src | data:text/javascript;charset=utf-8,B "
      "| data:text/javascript;charset=utf-8,C\n"
      "blinkSetAttribute | script | src | data:text/javascript;charset=utf-8,C "
      "| data:text/javascript;charset=utf-8,D\n"
      "blinkSetAttribute | script | src | data:text/javascript;charset=utf-8,D "
      "| data:text/javascript;charset=utf-8,E";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, IFrameSrcAttribute) {
  const char* code =
      "document.body.innerHTML = '<iframe "
      "src=\\\'data:text/html;charset=utf-8,A\\\'></iframe>';"
      "var iframe = document.getElementsByTagName('iframe')[0];"
      "iframe.src = 'data:text/html;charset=utf-8,B';"
      "iframe.setAttribute('src', 'data:text/html;charset=utf-8,C');"
      "iframe.setAttributeNS('', 'src', 'data:text/html;charset=utf-8,D');"
      "var attr = document.createAttribute('src');"
      "attr.value = 'data:text/html;charset=utf-8,E';"
      "iframe.setAttributeNode(attr);";
  const char* expected_activities =
      "blinkAddElement | iframe | data:text/html;charset=utf-8,A\n"
      "blinkRequestResource | Main resource | data:text/html;charset=utf-8,A\n"
      "blinkSetAttribute | iframe | src | data:text/html;charset=utf-8,A | "
      "data:text/html;charset=utf-8,B\n"
      "blinkRequestResource | Main resource | data:text/html;charset=utf-8,B\n"
      "blinkSetAttribute | iframe | src | data:text/html;charset=utf-8,B | "
      "data:text/html;charset=utf-8,C\n"
      "blinkRequestResource | Main resource | data:text/html;charset=utf-8,C\n"
      "blinkSetAttribute | iframe | src | data:text/html;charset=utf-8,C | "
      "data:text/html;charset=utf-8,D\n"
      "blinkRequestResource | Main resource | data:text/html;charset=utf-8,D\n"
      "blinkSetAttribute | iframe | src | data:text/html;charset=utf-8,D | "
      "data:text/html;charset=utf-8,E\n"
      "blinkRequestResource | Main resource | data:text/html;charset=utf-8,E";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, AnchorHrefAttribute) {
  const char* code =
      "document.body.innerHTML = '<a "
      "href=\\\'data:text/html;charset=utf-8,A\\\'></a>';"
      "var a = document.getElementsByTagName('a')[0];"
      "a.href = 'data:text/html;charset=utf-8,B';"
      "a.setAttribute('href', 'data:text/html;charset=utf-8,C');"
      "a.setAttributeNS('', 'href', 'data:text/html;charset=utf-8,D');"
      "var attr = document.createAttribute('href');"
      "attr.value = 'data:text/html;charset=utf-8,E';"
      "a.setAttributeNode(attr);";
  const char* expected_activities =
      "blinkAddElement | a | data:text/html;charset=utf-8,A\n"
      "blinkSetAttribute | a | href | data:text/html;charset=utf-8,A | "
      "data:text/html;charset=utf-8,B\n"
      "blinkSetAttribute | a | href | data:text/html;charset=utf-8,B | "
      "data:text/html;charset=utf-8,C\n"
      "blinkSetAttribute | a | href | data:text/html;charset=utf-8,C | "
      "data:text/html;charset=utf-8,D\n"
      "blinkSetAttribute | a | href | data:text/html;charset=utf-8,D | "
      "data:text/html;charset=utf-8,E";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, LinkHrefAttribute) {
  const char* code =
      "document.body.innerHTML = '<link rel=\\\'stylesheet\\\' "
      "href=\\\'data:text/css;charset=utf-8,A\\\'></link>';"
      "var link = document.getElementsByTagName('link')[0];"
      "link.href = 'data:text/css;charset=utf-8,B';"
      "link.setAttribute('href', 'data:text/css;charset=utf-8,C');"
      "link.setAttributeNS('', 'href', 'data:text/css;charset=utf-8,D');"
      "var attr = document.createAttribute('href');"
      "attr.value = 'data:text/css;charset=utf-8,E';"
      "link.setAttributeNode(attr);";
  const char* expected_activities =
      "blinkAddElement | link | stylesheet | data:text/css;charset=utf-8,A\n"
      "blinkRequestResource | CSS stylesheet | data:text/css;charset=utf-8,A\n"
      "blinkSetAttribute | link | href | data:text/css;charset=utf-8,A | "
      "data:text/css;charset=utf-8,B\n"
      "blinkRequestResource | CSS stylesheet | data:text/css;charset=utf-8,B\n"
      "blinkSetAttribute | link | href | data:text/css;charset=utf-8,B | "
      "data:text/css;charset=utf-8,C\n"
      "blinkRequestResource | CSS stylesheet | data:text/css;charset=utf-8,C\n"
      "blinkSetAttribute | link | href | data:text/css;charset=utf-8,C | "
      "data:text/css;charset=utf-8,D\n"
      "blinkRequestResource | CSS stylesheet | data:text/css;charset=utf-8,D\n"
      "blinkSetAttribute | link | href | data:text/css;charset=utf-8,D | "
      "data:text/css;charset=utf-8,E\n"
      "blinkRequestResource | CSS stylesheet | data:text/css;charset=utf-8,E";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, InputFormActionAttribute) {
  const char* code =
      "document.body.innerHTML = '<input type=\\\'button\\\' "
      "formaction=\\\'data:text/html;charset=utf-8,A\\\'></input>';"
      "var input = document.getElementsByTagName('input')[0];"
      "input.formAction = 'data:text/html;charset=utf-8,B';"
      "input.setAttribute('formaction', 'data:text/html;charset=utf-8,C');"
      "input.setAttributeNS('', 'formaction', "
      "'data:text/html;charset=utf-8,D');"
      "var attr = document.createAttribute('formaction');"
      "attr.value = 'data:text/html;charset=utf-8,E';"
      "input.setAttributeNode(attr);";
  const char* expected_activities =
      "blinkAddElement | input | button | data:text/html;charset=utf-8,A\n"
      "blinkSetAttribute | input | formaction | data:text/html;charset=utf-8,A "
      "| data:text/html;charset=utf-8,B\n"
      "blinkSetAttribute | input | formaction | data:text/html;charset=utf-8,B "
      "| data:text/html;charset=utf-8,C\n"
      "blinkSetAttribute | input | formaction | data:text/html;charset=utf-8,C "
      "| data:text/html;charset=utf-8,D\n"
      "blinkSetAttribute | input | formaction | data:text/html;charset=utf-8,D "
      "| data:text/html;charset=utf-8,E";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, ButtonFormActionAttribute) {
  const char* code =
      "document.body.innerHTML = '<button type=\\\'submit\\\' "
      "formmethod=\\\'post\\\' "
      "formaction=\\\'data:text/html;charset=utf-8,A\\\'></input>';"
      "var button = document.getElementsByTagName('button')[0];"
      "button.formAction = 'data:text/html;charset=utf-8,B';"
      "button.setAttribute('formaction', 'data:text/html;charset=utf-8,C');"
      "button.setAttributeNS('', 'formaction', "
      "'data:text/html;charset=utf-8,D');"
      "var attr = document.createAttribute('formaction');"
      "attr.value = 'data:text/html;charset=utf-8,E';"
      "button.setAttributeNode(attr);";
  const char* expected_activities =
      "blinkAddElement | button | submit | post | "
      "data:text/html;charset=utf-8,A\n"
      "blinkSetAttribute | button | formaction | "
      "data:text/html;charset=utf-8,A | data:text/html;charset=utf-8,B\n"
      "blinkSetAttribute | button | formaction | "
      "data:text/html;charset=utf-8,B | data:text/html;charset=utf-8,C\n"
      "blinkSetAttribute | button | formaction | "
      "data:text/html;charset=utf-8,C | data:text/html;charset=utf-8,D\n"
      "blinkSetAttribute | button | formaction | "
      "data:text/html;charset=utf-8,D | data:text/html;charset=utf-8,E";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, FormActionAttribute) {
  const char* code =
      "document.body.innerHTML = '<form "
      "action=\\\'data:text/html;charset=utf-8,A\\\'></form>';"
      "var form = document.getElementsByTagName('form')[0];"
      "form.action = 'data:text/html;charset=utf-8,B';"
      "form.setAttribute('action', 'data:text/html;charset=utf-8,C');"
      "form.setAttributeNS('', 'action', 'data:text/html;charset=utf-8,D');"
      "var attr = document.createAttribute('action');"
      "attr.value = 'data:text/html;charset=utf-8,E';"
      "form.setAttributeNode(attr);";
  const char* expected_activities =
      "blinkAddElement | form |  | data:text/html;charset=utf-8,A\n"
      "blinkSetAttribute | form | action | data:text/html;charset=utf-8,A | "
      "data:text/html;charset=utf-8,B\n"
      "blinkSetAttribute | form | action | data:text/html;charset=utf-8,B | "
      "data:text/html;charset=utf-8,C\n"
      "blinkSetAttribute | form | action | data:text/html;charset=utf-8,C | "
      "data:text/html;charset=utf-8,D\n"
      "blinkSetAttribute | form | action | data:text/html;charset=utf-8,D | "
      "data:text/html;charset=utf-8,E";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, LocalDOMWindowAttribute) {
  const char* code =
      "location.href = 'data:text/html;charset=utf-8,A';"
      "location.assign('data:text/html;charset=utf-8,B');"
      "location.replace('data:text/html;charset=utf-8,C');"
      "location.protocol = 'protocol';"
      "location.pathname = 'pathname';"
      "location.search = 'search';"
      "location.hash = 'hash';"
      "location.href = 'about:blank';";
  const char* expected_activities =
      "blinkSetAttribute | LocalDOMWindow | url | about:blank | "
      "data:text/html;charset=utf-8,A\n"
      "blinkRequestResource | Main resource | data:text/html;charset=utf-8,A\n"
      "blinkSetAttribute | LocalDOMWindow | url | about:blank | "
      "data:text/html;charset=utf-8,B\n"
      "blinkRequestResource | Main resource | data:text/html;charset=utf-8,B\n"
      "blinkSetAttribute | LocalDOMWindow | url | about:blank | "
      "data:text/html;charset=utf-8,C\n"
      "blinkRequestResource | Main resource | data:text/html;charset=utf-8,C\n"
      "blinkSetAttribute | LocalDOMWindow | url | about:blank | "
      "protocol:blank\n"
      "blinkRequestResource | Main resource | protocol:blank\n"
      "blinkSetAttribute | LocalDOMWindow | url | about:blank | "
      "about:pathname\n"
      "blinkSetAttribute | LocalDOMWindow | url | about:blank | "
      "about:blank?search\n"
      "blinkSetAttribute | LocalDOMWindow | url | about:blank | "
      "about:blank#hash\n"
      "blinkSetAttribute | LocalDOMWindow | url | about:blank#hash | "
      "about:blank\n";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

TEST_F(ActivityLoggerTest, RequestResource) {
  const char* code =
      "document.write('<iframe "
      "src=\\\'data:text/html;charset=utf-8,A\\\'></iframe>');"
      "document.write('<img "
      "src=\\\'data:text/html;charset=utf-8,B\\\'></img>');"
      "document.write('<link rel=\\\'stylesheet\\\' "
      "href=\\\'data:text/html;charset=utf-8,C\\\'></link>');"
      "document.write('<script "
      "src=\\\'data:text/html;charset=utf-8,D\\\'></script>');"
      "document.close();"
      "var xhr = new XMLHttpRequest(); xhr.open('GET', "
      "'data:text/html;charset=utf-8,E'); xhr.send();";
  const char* expected_activities =
      "blinkAddElement | iframe | data:text/html;charset=utf-8,A\n"
      "blinkRequestResource | Main resource | data:text/html;charset=utf-8,A\n"
      "blinkAddElement | link | stylesheet | data:text/html;charset=utf-8,C\n"
      "blinkRequestResource | CSS stylesheet | data:text/html;charset=utf-8,C\n"
      "blinkAddElement | script | data:text/html;charset=utf-8,D\n"
      "blinkRequestResource | Script | data:text/html;charset=utf-8,D\n"
      "blinkRequestResource | XMLHttpRequest | data:text/html;charset=utf-8,E\n"
      "blinkRequestResource | Image | data:text/html;charset=utf-8,B\n";
  ExecuteScriptInMainWorld(code);
  ASSERT_TRUE(VerifyActivities(""));
  ExecuteScriptInIsolatedWorld(code);
  ASSERT_TRUE(VerifyActivities(expected_activities));
}

}  // namespace blink
