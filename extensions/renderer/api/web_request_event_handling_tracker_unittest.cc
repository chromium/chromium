// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/web_request_event_handling_tracker.h"

#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "extensions/common/api/web_request/web_request_constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kEventName[] = "webRequest.onBeforeRequest";
constexpr uint64_t kRequestId = 42;
constexpr int kWebViewInstanceId = 7;

// Returns the [details, payload] argument list of a per-context webRequest
// event dispatch.
base::ListValue MakeEventArgs(bool await_response) {
  base::DictValue details;
  details.Set("requestId", base::NumberToString(kRequestId));
  base::DictValue payload;
  payload.Set(kContextDispatchAwaitResponseKey, await_response);
  payload.Set(kContextDispatchInstanceIdKey, kWebViewInstanceId);
  base::ListValue args;
  args.Append(std::move(details));
  args.Append(std::move(payload));
  return args;
}

}  // namespace

class WebRequestEventHandlingTrackerUnittest
    : public NativeExtensionBindingsSystemUnittest {
 public:
  void SetUp() override {
    NativeExtensionBindingsSystemUnittest::SetUp();

    extension_ = ExtensionBuilder("foo").Build();
    RegisterExtension(extension_);

    v8::HandleScope handle_scope(isolate());
    script_context_ =
        CreateScriptContext(MainContext(), extension_.get(),
                            mojom::ContextType::kPrivilegedExtension);
  }

  void TearDown() override {
    script_context_ = nullptr;
    extension_ = nullptr;
    NativeExtensionBindingsSystemUnittest::TearDown();
  }

  // Make unexpected completion signals fail immediately.
  bool UseStrictIPCMessageSender() override { return true; }

  WebRequestEventHandlingTracker& tracker() {
    return *bindings_system()->web_request_event_handling_tracker();
  }
  ScriptContext* script_context() { return script_context_; }
  const Extension* extension() const { return extension_.get(); }

  WebRequestEventHandlingTracker::DispatchInfo DispatchInfoForExtension()
      const {
    return {extension_->id(), kEventName, kRequestId,
            /*web_view_instance_id=*/0};
  }

  // Expects exactly one completion signal for `info`.
  void ExpectSignal(const WebRequestEventHandlingTracker::DispatchInfo& info) {
    EXPECT_CALL(*ipc_message_sender(),
                SendWebRequestEventHandlingDoneIPC(
                    info.extension_id, info.event_name, info.request_id,
                    info.web_view_instance_id));
  }

 private:
  scoped_refptr<const Extension> extension_;
  raw_ptr<ScriptContext> script_context_ = nullptr;
};

// The signal waits until every listener was notified and the last context
// reported: an early, synchronous report must not complete the dispatch. A
// completed dispatch sends exactly one signal, and a repeated report sends
// nothing more.
TEST_F(WebRequestEventHandlingTrackerUnittest, SignalsAfterLastContext) {
  v8::HandleScope handle_scope(isolate());
  ScriptContext* context_b = CreateScriptContext(
      AddContext(), extension(), mojom::ContextType::kPrivilegedExtension);

  const auto info = DispatchInfoForExtension();

  // No signal yet: not every listener was notified, then context_b has not
  // reported.
  tracker().ExpectReportFrom(*script_context(), info);
  tracker().OnContextReported(*script_context(), info);
  tracker().ExpectReportFrom(*context_b, info);
  tracker().OnAllListenersNotified(info);

  // The last report completes the dispatch; the repeated report after it must
  // send nothing more.
  ExpectSignal(info);
  tracker().OnContextReported(*context_b, info);
  tracker().OnContextReported(*context_b, info);
}

// A dispatch that expects no report (e.g. no context had a matching listener)
// must still send the signal the browser awaits.
TEST_F(WebRequestEventHandlingTrackerUnittest, SignalsWithoutExpectedContexts) {
  const auto info = DispatchInfoForExtension();
  ExpectSignal(info);
  tracker().OnAllListenersNotified(info);
}

// A context that dies before it reports must not block the dispatch: the
// tracker watches each expected context for invalidation, and the signal goes
// out when the last one goes away.
TEST_F(WebRequestEventHandlingTrackerUnittest, SignalsAfterInvalidation) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> v8_context_b = AddContext();
  ScriptContext* context_b = CreateScriptContext(
      v8_context_b, extension(), mojom::ContextType::kPrivilegedExtension);

  const auto info = DispatchInfoForExtension();
  tracker().ExpectReportFrom(*context_b, info);
  tracker().OnAllListenersNotified(info);

  ExpectSignal(info);
  DisposeContext(v8_context_b);
}

// GetBlockingDispatchInfo() runs on every event dispatch: it must accept a
// blocking webRequest dispatch and reject other events and non-blocking
// dispatches.
TEST(WebRequestEventHandlingTrackerTest, GetBlockingDispatchInfo) {
  base::test::ScopedFeatureList scoped_feature_list(
      extensions_features::kWebRequestPerContextEventDispatch);

  const ExtensionId extension_id(32, 'a');
  std::optional<WebRequestEventHandlingTracker::DispatchInfo> info =
      WebRequestEventHandlingTracker::GetBlockingDispatchInfo(
          extension_id, kEventName, MakeEventArgs(/*await_response=*/true));
  ASSERT_TRUE(info);
  EXPECT_EQ(extension_id, info->extension_id);
  EXPECT_EQ(kEventName, info->event_name);
  EXPECT_EQ(kRequestId, info->request_id);
  EXPECT_EQ(kWebViewInstanceId, info->web_view_instance_id);

  // A WebUI webview embedder has no extension.
  info = WebRequestEventHandlingTracker::GetBlockingDispatchInfo(
      /*extension_id=*/std::nullopt, "webViewInternal.onBeforeRequest",
      MakeEventArgs(/*await_response=*/true));
  ASSERT_TRUE(info);
  EXPECT_EQ(std::nullopt, info->extension_id);

  EXPECT_FALSE(WebRequestEventHandlingTracker::GetBlockingDispatchInfo(
      extension_id, "tabs.onUpdated", MakeEventArgs(/*await_response=*/true)));
  EXPECT_FALSE(WebRequestEventHandlingTracker::GetBlockingDispatchInfo(
      extension_id, kEventName, MakeEventArgs(/*await_response=*/false)));
}

// The feature gates the whole path: when it's off, nothing is a blocking
// dispatch.
TEST(WebRequestEventHandlingTrackerTest, GetBlockingDispatchInfoFeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      extensions_features::kWebRequestPerContextEventDispatch);

  EXPECT_FALSE(WebRequestEventHandlingTracker::GetBlockingDispatchInfo(
      ExtensionId(32, 'a'), kEventName,
      MakeEventArgs(/*await_response=*/true)));
}

}  // namespace extensions
