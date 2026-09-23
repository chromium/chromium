// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/diagnostic_logging/rtc_diagnostic_logging.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/uuid.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/webrtc/rtc_logging_utils.h"
#include "third_party/blink/public/mojom/webrtc/rtc_logging.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_diagnostic_logging_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class FakeRTCLoggingDispatcher : public mojom::blink::RTCLoggingDispatcher {
 public:
  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<mojom::blink::RTCLoggingDispatcher>(
        std::move(handle)));
  }

  void StartDiagnosticLogging(
      const base::Uuid& session_id,
      bool upload,
      const HashMap<String, String>& metadata,
      StartDiagnosticLoggingCallback callback) override {
    uuid_ = String(session_id.AsLowercaseString());
    upload_ = upload;
    metadata_ = metadata;
    std::move(callback).Run();
  }

  void FinishDiagnosticLogging(
      const HashMap<String, String>& metadata,
      FinishDiagnosticLoggingCallback callback) override {
    finish_called_ = true;
    finish_metadata_ = metadata;
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
  const HashMap<String, String>& finish_metadata() const {
    return finish_metadata_;
  }
  const String& uuid() const { return uuid_; }

 private:
  mojo::Receiver<mojom::blink::RTCLoggingDispatcher> receiver_{this};
  bool upload_ = false;
  HashMap<String, String> metadata_;
  HashMap<String, String> finish_metadata_;
  bool finish_called_ = false;
  bool cancel_called_ = false;
  String uuid_;
};

class RTCPeerConnectionDiagnosticLoggingTest : public testing::Test {
 public:
  RTCPeerConnectionDiagnosticLoggingTest()
      : holder_(std::make_unique<DummyPageHolder>()),
        handle_scope_(GetScriptState()->GetIsolate()),
        context_(GetScriptState()->GetContext()),
        context_scope_(context_) {}

  ScriptState* GetScriptState() const {
    return ToScriptStateForMainWorld(&holder_->GetFrame());
  }

  FakeRTCLoggingDispatcher& fake_dispatcher() { return fake_dispatcher_; }
  test::TaskEnvironment& task_environment() { return task_environment_; }

  template <typename Action>
  void ExpectThrowsInvalidStateOnDetachedFrame(Action action) {
    auto detached_holder = std::make_unique<DummyPageHolder>();
    ScriptState* detached_script_state =
        ToScriptStateForMainWorld(&detached_holder->GetFrame());
    detached_holder->GetFrame().Detach(FrameDetachType::kRemove);

    DummyExceptionStateForTesting exception_state;
    action(detached_script_state, exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.Code(),
              ToExceptionCode(DOMExceptionCode::kInvalidStateError));
  }

 protected:
  void SetUp() override {
    holder_->GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::RTCLoggingDispatcher::Name_,
        base::BindRepeating(&FakeRTCLoggingDispatcher::Bind,
                            base::Unretained(&fake_dispatcher_)));
  }

  void TearDown() override {
    holder_->GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::RTCLoggingDispatcher::Name_, {});
  }

 private:
  test::TaskEnvironment task_environment_;
  FakeRTCLoggingDispatcher fake_dispatcher_;
  std::unique_ptr<DummyPageHolder> holder_;
  v8::HandleScope handle_scope_;
  v8::Local<v8::Context> context_;
  v8::Context::Scope context_scope_;
};

TEST_F(RTCPeerConnectionDiagnosticLoggingTest, StartDiagnosticLogging) {
  auto* options = RTCDiagnosticLoggingOptions::Create();
  Vector<std::pair<String, String>> metadata;
  metadata.push_back(std::make_pair(String("key"), String("value")));
  options->setMetadata(metadata);

  DummyExceptionStateForTesting exception_state;
  String session_id = RTCDiagnosticLogging::startDiagnosticLogging(
      GetScriptState(), options, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(base::Uuid::ParseLowercase(session_id.Utf8()).is_valid());

  task_environment().RunUntilIdle();
  EXPECT_EQ(session_id, fake_dispatcher().uuid());
  EXPECT_TRUE(fake_dispatcher().upload());
  EXPECT_EQ(fake_dispatcher().metadata().at("key"), "value");
}

TEST_F(RTCPeerConnectionDiagnosticLoggingTest,
       StartDiagnosticLoggingDefaultOptions) {
  auto* options = RTCDiagnosticLoggingOptions::Create();

  DummyExceptionStateForTesting exception_state;
  String session_id = RTCDiagnosticLogging::startDiagnosticLogging(
      GetScriptState(), options, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(base::Uuid::ParseLowercase(session_id.Utf8()).is_valid());

  task_environment().RunUntilIdle();
  EXPECT_EQ(session_id, fake_dispatcher().uuid());
  EXPECT_TRUE(fake_dispatcher().upload());
  EXPECT_TRUE(fake_dispatcher().metadata().empty());
}

TEST_F(RTCPeerConnectionDiagnosticLoggingTest,
       StartDiagnosticLoggingAlreadyStarted) {
  auto* options = RTCDiagnosticLoggingOptions::Create();

  DummyExceptionStateForTesting exception_state;
  RTCDiagnosticLogging::startDiagnosticLogging(GetScriptState(), options,
                                               exception_state);
  ASSERT_FALSE(exception_state.HadException());

  String second_id = RTCDiagnosticLogging::startDiagnosticLogging(
      GetScriptState(), options, exception_state);
  EXPECT_TRUE(second_id.IsNull());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(),
            ToExceptionCode(DOMExceptionCode::kInvalidStateError));
}

TEST_F(RTCPeerConnectionDiagnosticLoggingTest,
       StartDiagnosticLoggingMetadataTooLarge) {
  auto* options = RTCDiagnosticLoggingOptions::Create();
  Vector<std::pair<String, String>> metadata;
  for (size_t i = 0; i < RTCMetadataValidator::kMaxMetadataSize + 1; ++i) {
    metadata.push_back(std::make_pair(String::Number(i), String::Number(i)));
  }
  options->setMetadata(metadata);

  DummyExceptionStateForTesting exception_state;
  String session_id = RTCDiagnosticLogging::startDiagnosticLogging(
      GetScriptState(), options, exception_state);
  EXPECT_TRUE(session_id.IsNull());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(), ToExceptionCode(ESErrorType::kTypeError));
  EXPECT_EQ(exception_state.Message(), "Too many metadata entries.");
}

TEST_F(RTCPeerConnectionDiagnosticLoggingTest,
       StartDiagnosticLoggingMetadataEntryTooLong) {
  auto* options = RTCDiagnosticLoggingOptions::Create();
  Vector<std::pair<String, String>> metadata;
  StringBuilder builder;
  for (size_t i = 0; i < RTCMetadataValidator::kMaxMetadataLength + 1; ++i) {
    builder.Append('a');
  }
  metadata.push_back(std::make_pair(builder.ToString(), String("value")));
  options->setMetadata(metadata);

  DummyExceptionStateForTesting exception_state;
  String session_id = RTCDiagnosticLogging::startDiagnosticLogging(
      GetScriptState(), options, exception_state);
  EXPECT_TRUE(session_id.IsNull());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(), ToExceptionCode(ESErrorType::kTypeError));
  EXPECT_EQ(exception_state.Message(), "Metadata entry too long.");
}

TEST_F(RTCPeerConnectionDiagnosticLoggingTest,
       StartDiagnosticLoggingMetadataEntryTooLongUtf8) {
  auto* options = RTCDiagnosticLoggingOptions::Create();
  Vector<std::pair<String, String>> metadata;
  StringBuilder builder;
  for (size_t i = 0; i < RTCMetadataValidator::kMaxMetadataLength - 1; ++i) {
    builder.Append('a');
  }
  builder.Append(
      String::FromUtf8(base::as_bytes(base::span_from_cstring("é"))));

  metadata.push_back(std::make_pair(builder.ToString(), String("value")));
  options->setMetadata(metadata);

  DummyExceptionStateForTesting exception_state;
  String session_id = RTCDiagnosticLogging::startDiagnosticLogging(
      GetScriptState(), options, exception_state);
  EXPECT_TRUE(session_id.IsNull());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(), ToExceptionCode(ESErrorType::kTypeError));
  EXPECT_EQ(exception_state.Message(), "Metadata entry too long.");
}

TEST_F(RTCPeerConnectionDiagnosticLoggingTest, StopDiagnosticLogging) {
  auto* options = RTCDiagnosticLoggingOptions::Create();
  DummyExceptionStateForTesting exception_state;

  RTCDiagnosticLogging::startDiagnosticLogging(GetScriptState(), options,
                                               exception_state);
  ASSERT_FALSE(exception_state.HadException());

  RTCDiagnosticLogging::stopDiagnosticLogging(GetScriptState(),
                                              exception_state);
  EXPECT_FALSE(exception_state.HadException());

  task_environment().RunUntilIdle();
  EXPECT_TRUE(fake_dispatcher().finish_called());
  EXPECT_TRUE(fake_dispatcher().finish_metadata().empty());

  // Can start a new session after stopping.
  String new_id = RTCDiagnosticLogging::startDiagnosticLogging(
      GetScriptState(), options, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(base::Uuid::ParseLowercase(new_id.Utf8()).is_valid());
}

TEST_F(RTCPeerConnectionDiagnosticLoggingTest,
       StopDiagnosticLoggingWithoutStart) {
  DummyExceptionStateForTesting exception_state;
  RTCDiagnosticLogging::stopDiagnosticLogging(GetScriptState(),
                                              exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(),
            ToExceptionCode(DOMExceptionCode::kInvalidStateError));
}

TEST_F(RTCPeerConnectionDiagnosticLoggingTest,
       StopDiagnosticLoggingAlreadyStopped) {
  auto* options = RTCDiagnosticLoggingOptions::Create();
  DummyExceptionStateForTesting exception_state;

  RTCDiagnosticLogging::startDiagnosticLogging(GetScriptState(), options,
                                               exception_state);
  ASSERT_FALSE(exception_state.HadException());

  RTCDiagnosticLogging::stopDiagnosticLogging(GetScriptState(),
                                              exception_state);
  ASSERT_FALSE(exception_state.HadException());

  RTCDiagnosticLogging::stopDiagnosticLogging(GetScriptState(),
                                              exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(),
            ToExceptionCode(DOMExceptionCode::kInvalidStateError));
}

TEST_F(RTCPeerConnectionDiagnosticLoggingTest, CancelDiagnosticLogging) {
  auto* options = RTCDiagnosticLoggingOptions::Create();
  DummyExceptionStateForTesting exception_state;

  RTCDiagnosticLogging::startDiagnosticLogging(GetScriptState(), options,
                                               exception_state);
  ASSERT_FALSE(exception_state.HadException());

  RTCDiagnosticLogging::cancelDiagnosticLogging(GetScriptState(),
                                                exception_state);
  EXPECT_FALSE(exception_state.HadException());

  task_environment().RunUntilIdle();
  EXPECT_TRUE(fake_dispatcher().cancel_called());

  // Can start a new session after cancelling.
  String new_id = RTCDiagnosticLogging::startDiagnosticLogging(
      GetScriptState(), options, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(base::Uuid::ParseLowercase(new_id.Utf8()).is_valid());
}

TEST_F(RTCPeerConnectionDiagnosticLoggingTest,
       CancelDiagnosticLoggingWithoutStart) {
  DummyExceptionStateForTesting exception_state;
  RTCDiagnosticLogging::cancelDiagnosticLogging(GetScriptState(),
                                                exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(),
            ToExceptionCode(DOMExceptionCode::kInvalidStateError));
}

TEST_F(RTCPeerConnectionDiagnosticLoggingTest,
       CancelDiagnosticLoggingAlreadyCancelled) {
  auto* options = RTCDiagnosticLoggingOptions::Create();
  DummyExceptionStateForTesting exception_state;

  RTCDiagnosticLogging::startDiagnosticLogging(GetScriptState(), options,
                                               exception_state);
  ASSERT_FALSE(exception_state.HadException());

  RTCDiagnosticLogging::cancelDiagnosticLogging(GetScriptState(),
                                                exception_state);
  ASSERT_FALSE(exception_state.HadException());

  RTCDiagnosticLogging::cancelDiagnosticLogging(GetScriptState(),
                                                exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(),
            ToExceptionCode(DOMExceptionCode::kInvalidStateError));
}

TEST_F(RTCPeerConnectionDiagnosticLoggingTest,
       StartDiagnosticLoggingOnDetachedFrame) {
  auto* options = RTCDiagnosticLoggingOptions::Create();
  ExpectThrowsInvalidStateOnDetachedFrame(
      [&](ScriptState* state, ExceptionState& exception_state) {
        RTCDiagnosticLogging::startDiagnosticLogging(state, options,
                                                     exception_state);
      });
}

TEST_F(RTCPeerConnectionDiagnosticLoggingTest,
       StopDiagnosticLoggingOnDetachedFrame) {
  ExpectThrowsInvalidStateOnDetachedFrame(
      [](ScriptState* state, ExceptionState& exception_state) {
        RTCDiagnosticLogging::stopDiagnosticLogging(state, exception_state);
      });
}

TEST_F(RTCPeerConnectionDiagnosticLoggingTest,
       CancelDiagnosticLoggingOnDetachedFrame) {
  ExpectThrowsInvalidStateOnDetachedFrame(
      [](ScriptState* state, ExceptionState& exception_state) {
        RTCDiagnosticLogging::cancelDiagnosticLogging(state, exception_state);
      });
}

}  // namespace

}  // namespace blink
