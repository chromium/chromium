// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/media_key_session.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_content_decryption_module.h"
#include "third_party/blink/public/platform/web_content_decryption_module_session.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_encrypted_media_key_information.h"
#include "third_party/blink/public/platform/web_encrypted_media_types.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_key_session.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_keys.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_status_map.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

namespace {

// Ref-counted state shared between the test and fake CDM objects so pointers
// stay valid even if Oilpan finalizes MediaKeys during test teardown.
struct TestSessionState : public base::RefCounted<TestSessionState> {
  raw_ptr<WebContentDecryptionModuleSession::Client> client = nullptr;
  bool session_destroyed = false;

 private:
  friend class base::RefCounted<TestSessionState>;
  ~TestSessionState() = default;
};

// Minimal fake CDM session that captures the MediaKeySession client pointer
// and tracks when MediaKeySession destroys the underlying session.
class FakeWebContentDecryptionModuleSession
    : public WebContentDecryptionModuleSession {
 public:
  explicit FakeWebContentDecryptionModuleSession(
      scoped_refptr<TestSessionState> state)
      : state_(std::move(state)) {}

  ~FakeWebContentDecryptionModuleSession() override {
    state_->session_destroyed = true;
  }

  void SetClientInterface(Client* client) override { state_->client = client; }

  WebString SessionId() const override { return WebString::FromUtf8("test"); }

  void InitializeNewSession(media::EmeInitDataType,
                            base::span<const uint8_t>,
                            WebContentDecryptionModuleResult) override {}
  void Load(const WebString&, WebContentDecryptionModuleResult) override {}
  void Update(base::span<const uint8_t>,
              WebContentDecryptionModuleResult) override {}
  void Close(WebContentDecryptionModuleResult) override {}
  void Remove(WebContentDecryptionModuleResult) override {}

 private:
  scoped_refptr<TestSessionState> state_;
};

class FakeWebContentDecryptionModule : public WebContentDecryptionModule {
 public:
  explicit FakeWebContentDecryptionModule(scoped_refptr<TestSessionState> state)
      : state_(std::move(state)) {}

  std::unique_ptr<WebContentDecryptionModuleSession> CreateSession(
      WebEncryptedMediaSessionType) override {
    return std::make_unique<FakeWebContentDecryptionModuleSession>(state_);
  }

  void SetServerCertificate(base::span<const uint8_t>,
                            WebContentDecryptionModuleResult) override {}
  void GetStatusForPolicy(const WebString&,
                          WebContentDecryptionModuleResult) override {}

 private:
  scoped_refptr<TestSessionState> state_;
};

std::vector<WebEncryptedMediaKeyInformation> MakeSampleKeys(
    uint32_t system_code) {
  std::vector<WebEncryptedMediaKeyInformation> keys(1);
  keys[0].SetId(WebData(base::byte_span_from_cstring("key1")));
  keys[0].SetStatus(WebEncryptedMediaKeyInformation::KeyStatus::kUsable);
  keys[0].SetSystemCode(system_code);
  return keys;
}

}  // namespace

class MediaKeySessionTest : public SimTest {
 protected:
  MediaKeys* CreateMediaKeys(ExecutionContext* context,
                             scoped_refptr<TestSessionState> state) {
    MediaKeysConfig config = {"org.w3.clearkey", false};
    return MakeGarbageCollected<MediaKeys>(
        context,
        std::vector<WebEncryptedMediaSessionType>{
            WebEncryptedMediaSessionType::kTemporary},
        std::make_unique<FakeWebContentDecryptionModule>(std::move(state)),
        config);
  }

  // Invokes createSession() in |eval_script_state| on a pre-wrapped MediaKeys
  // V8 object (which may belong to a different, possibly destroyed realm).
  MediaKeySession* InvokeCreateSessionViaJs(
      ScriptState* eval_script_state,
      const v8::Global<v8::Value>& v8_media_keys_global) {
    v8::Isolate* isolate = eval_script_state->GetIsolate();
    ScriptState::Scope eval_scope(eval_script_state);
    v8::Local<v8::Context> eval_context = eval_script_state->GetContext();
    v8::MicrotasksScope microtasks_scope(
        eval_context, v8::MicrotasksScope::kDoNotRunMicrotasks);

    EXPECT_TRUE(eval_context->Global()
                    ->Set(eval_context,
                          V8AtomicString(isolate, "wrappedMediaKeys"),
                          v8_media_keys_global.Get(isolate))
                    .FromMaybe(false));

    ScriptEvaluationResult eval_result =
        ClassicScript::CreateUnspecifiedScript(
            "wrappedMediaKeys.createSession()")
            ->RunScriptOnScriptStateAndReturnValue(eval_script_state);
    EXPECT_EQ(eval_result.GetResultType(),
              ScriptEvaluationResult::ResultType::kSuccess);

    MediaKeySession* session = nullptr;
    if (eval_result.GetResultType() ==
        ScriptEvaluationResult::ResultType::kSuccess) {
      session = V8MediaKeySession::ToWrappable(isolate,
                                               eval_result.GetSuccessValue());
    }

    // Clear the global property so nothing in JS retains the wrapper's realm.
    EXPECT_TRUE(
        eval_context->Global()
            ->Delete(eval_context, V8AtomicString(isolate, "wrappedMediaKeys"))
            .FromMaybe(false));
    return session;
  }

  ukm::TestAutoSetUkmRecorder ukm_recorder_;
};

// Regression test for https://crbug.com/589815781: MediaKeys::createSession()
// previously bound the new MediaKeySession to the receiver V8 wrapper's realm
// rather than MediaKeys' own ExecutionContext. When a MediaKeys wrapper was
// recreated in a detached iframe's realm, MediaKeySession observed an
// already-destroyed ExecutionContext whose Document (and owned UkmRecorder)
// could be collected during OnSessionKeysChange().
TEST_F(MediaKeySessionTest, CreateSessionUsesMediaKeysExecutionContext) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"(
    <iframe id="test-iframe" src="about:blank"></iframe>
  )");

  Persistent<LocalDOMWindow> main_window = Window().GetFrame()->DomWindow();
  ASSERT_TRUE(main_window);

  auto state = base::MakeRefCounted<TestSessionState>();
  Persistent<MediaKeys> media_keys = CreateMediaKeys(main_window.Get(), state);

  ScriptState* main_script_state =
      ToScriptStateForMainWorld(Window().GetFrame());
  v8::Isolate* isolate = main_script_state->GetIsolate();

  // Wrap |media_keys| in the iframe's realm, then detach the iframe before
  // invoking createSession() from the main window.
  Persistent<MediaKeySession> session;
  {
    auto* iframe = To<HTMLIFrameElement>(
        GetDocument().getElementById(AtomicString("test-iframe")));
    ASSERT_TRUE(iframe);
    auto* child_frame = To<LocalFrame>(iframe->ContentFrame());
    ASSERT_TRUE(child_frame);
    LocalDOMWindow* child_window = child_frame->DomWindow();
    ASSERT_TRUE(child_window);

    ScriptState* child_script_state = ToScriptStateForMainWorld(child_frame);
    v8::Global<v8::Value> v8_media_keys_global;
    {
      ScriptState::Scope child_scope(child_script_state);
      v8::Local<v8::Value> v8_media_keys =
          ToV8Traits<MediaKeys>::ToV8(child_script_state, media_keys.Get());
      ASSERT_FALSE(v8_media_keys.IsEmpty());
      v8_media_keys_global.Reset(isolate, v8_media_keys);
    }

    iframe->remove();
    ASSERT_TRUE(child_window->IsContextDestroyed());
    ASSERT_FALSE(main_window->IsContextDestroyed());

    session = InvokeCreateSessionViaJs(main_script_state, v8_media_keys_global);
  }

  ASSERT_TRUE(session);
  ASSERT_TRUE(state->client);

  // MediaKeySession must observe MediaKeys' alive ExecutionContext rather than
  // the destroyed iframe's ExecutionContext.
  EXPECT_EQ(session->GetExecutionContext(), main_window.Get());
  ASSERT_TRUE(session->GetExecutionContext());
  EXPECT_FALSE(session->GetExecutionContext()->IsContextDestroyed());

  // Collect the detached iframe's Window and Document.
  isolate->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);

  // OnSessionKeysChange must safely update keyStatuses and record UKM on the
  // main window without dereferencing a null or freed UkmRecorder.
  state->client->OnSessionKeysChange(MakeSampleKeys(/*system_code=*/42),
                                     /*has_additional_usable_key=*/true);

  EXPECT_EQ(session->keyStatuses()->size(), 1u);
  auto entries = ukm_recorder_.GetEntriesByName(
      ukm::builders::Media_EME_CdmSystemCode::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0]->source_id, main_window->UkmSourceID());
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entries[0], ukm::builders::Media_EME_CdmSystemCode::kCdmSystemCodeName,
      42);

  // Destroying MediaKeys' context must also notify MediaKeySession and tear
  // down the underlying CDM session.
  EXPECT_FALSE(state->session_destroyed);
  LoadURL("about:blank");
  EXPECT_TRUE(state->session_destroyed);
  EXPECT_EQ(session->GetExecutionContext(), nullptr);

  // Clear raw client pointer before releasing |session|.
  state->client = nullptr;
}

}  // namespace blink
