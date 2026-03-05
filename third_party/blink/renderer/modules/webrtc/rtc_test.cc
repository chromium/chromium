// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webrtc/rtc.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/rtc_logging/rtc_logging.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_diagnostic_logging_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

bool IsRangeError(ScriptState* script_state,
                  ScriptValue value,
                  const String& message) {
  v8::Local<v8::Object> object;
  if (!value.V8Value()->ToObject(script_state->GetContext()).ToLocal(&object)) {
    return false;
  }
  if (!object->IsNativeError()) {
    return false;
  }

  const auto& Has = [script_state, object](const String& key,
                                           const String& value) -> bool {
    v8::Local<v8::Value> actual;
    return object
               ->Get(script_state->GetContext(),
                     V8AtomicString(script_state->GetIsolate(), key))
               .ToLocal(&actual) &&
           ToCoreStringWithUndefinedOrNullCheck(script_state->GetIsolate(),
                                                actual) == value;
  };

  return Has("name", "RangeError") && Has("message", message);
}

class MockRTCLoggingDispatcher : public mojom::blink::RTCLoggingDispatcher {
 public:
  MockRTCLoggingDispatcher() = default;
  ~MockRTCLoggingDispatcher() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<mojom::blink::RTCLoggingDispatcher>(
        std::move(handle)));
  }

  void StartDiagnosticLogging(
      bool upload,
      const HashMap<String, String>& metadata,
      StartDiagnosticLoggingCallback callback) override {
    upload_ = upload;
    metadata_ = metadata;
    std::move(callback).Run(uuid_);
  }

  void FinishDiagnosticLogging(
      FinishDiagnosticLoggingCallback callback) override {
    finish_called_ = true;
    std::move(callback).Run();
  }

  void CancelDiagnosticLogging(
      CancelDiagnosticLoggingCallback callback) override {
    cancel_called_ = true;
    std::move(callback).Run();
  }

  bool upload() const { return upload_; }
  const HashMap<String, String>& metadata() const { return metadata_; }
  bool finish_called() const { return finish_called_; }
  bool cancel_called() const { return cancel_called_; }
  const String& uuid() const { return uuid_; }
  void set_uuid(const String& uuid) { uuid_ = uuid; }

 private:
  mojo::Receiver<mojom::blink::RTCLoggingDispatcher> receiver_{this};
  bool upload_ = false;
  HashMap<String, String> metadata_;
  bool finish_called_ = false;
  bool cancel_called_ = false;
  String uuid_ = "test-uuid";
};

class RTCTest : public testing::Test {
 public:
  RTCTest()
      : holder_(std::make_unique<DummyPageHolder>()),
        handle_scope_(GetScriptState()->GetIsolate()),
        context_(GetScriptState()->GetContext()),
        context_scope_(context_) {}

  ScriptState* GetScriptState() const {
    return ToScriptStateForMainWorld(&holder_->GetFrame());
  }

  RTC* GetRTC() {
    return RTC::rtc(*holder_->GetFrame().DomWindow()->navigator());
  }

  MockRTCLoggingDispatcher& mock_dispatcher() { return mock_dispatcher_; }

 protected:
  void SetUp() override {
    holder_->GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::RTCLoggingDispatcher::Name_,
        base::BindRepeating(&MockRTCLoggingDispatcher::Bind,
                            base::Unretained(&mock_dispatcher_)));
  }

  void TearDown() override {
    holder_->GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::RTCLoggingDispatcher::Name_, {});
  }

 public:
  test::TaskEnvironment task_environment;
  MockRTCLoggingDispatcher mock_dispatcher_;
  std::unique_ptr<DummyPageHolder> holder_;
  v8::HandleScope handle_scope_;
  v8::Local<v8::Context> context_;
  v8::Context::Scope context_scope_;
};

TEST_F(RTCTest, StartDiagnosticLogging) {
  auto* options = RTCDiagnosticLoggingOptions::Create();
  options->setAllowUpload(true);
  Vector<std::pair<String, String>> metadata;
  metadata.push_back(std::make_pair(String("key"), String("value")));
  options->setMetadata(metadata);

  auto promise = GetRTC()->startDiagnosticLogging(GetScriptState(), options);
  ScriptPromiseTester tester(GetScriptState(), promise);

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_EQ(tester.ValueAsString(), mock_dispatcher().uuid());
  EXPECT_TRUE(mock_dispatcher().upload());
  EXPECT_EQ(mock_dispatcher().metadata().at("key"), "value");
}

TEST_F(RTCTest, StartDiagnosticLoggingDefaultOptions) {
  auto* options = RTCDiagnosticLoggingOptions::Create();

  auto promise = GetRTC()->startDiagnosticLogging(GetScriptState(), options);
  ScriptPromiseTester tester(GetScriptState(), promise);

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_EQ(tester.ValueAsString(), mock_dispatcher().uuid());
  EXPECT_FALSE(mock_dispatcher().upload());
  EXPECT_TRUE(mock_dispatcher().metadata().empty());
}

TEST_F(RTCTest, StartDiagnosticLoggingMetadataTooLarge) {
  auto* options = RTCDiagnosticLoggingOptions::Create();
  Vector<std::pair<String, String>> metadata;
  for (size_t i = 0; i < RTC::kMaxMetadataSize + 1; ++i) {
    metadata.push_back(std::make_pair(String::Number(i), String::Number(i)));
  }
  options->setMetadata(metadata);

  auto promise = GetRTC()->startDiagnosticLogging(GetScriptState(), options);
  ScriptPromiseTester tester(GetScriptState(), promise);

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  EXPECT_TRUE(IsRangeError(GetScriptState(), tester.Value(),
                           "Too many metadata entries."));
}

TEST_F(RTCTest, StartDiagnosticLoggingMetadataEntryTooLong) {
  auto* options = RTCDiagnosticLoggingOptions::Create();
  Vector<std::pair<String, String>> metadata;
  StringBuilder builder;
  for (size_t i = 0; i < RTC::kMaxMetadataLength + 1; ++i) {
    builder.Append('a');
  }
  metadata.push_back(std::make_pair(builder.ToString(), String("value")));
  options->setMetadata(metadata);

  auto promise = GetRTC()->startDiagnosticLogging(GetScriptState(), options);
  ScriptPromiseTester tester(GetScriptState(), promise);

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  EXPECT_TRUE(IsRangeError(GetScriptState(), tester.Value(),
                           "Metadata entry too long."));
}

TEST_F(RTCTest, FinishDiagnosticLogging) {
  auto promise = GetRTC()->finishDiagnosticLogging(GetScriptState());
  ScriptPromiseTester tester(GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(mock_dispatcher().finish_called());
}

TEST_F(RTCTest, CancelDiagnosticLogging) {
  auto promise = GetRTC()->cancelDiagnosticLogging(GetScriptState());
  ScriptPromiseTester tester(GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(mock_dispatcher().cancel_called());
}

}  // namespace

}  // namespace blink
