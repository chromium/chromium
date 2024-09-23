// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/module_script.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/script/value_wrapper_synthetic_module_script.h"
#include "third_party/blink/renderer/core/testing/dummy_modulator.h"
#include "third_party/blink/renderer/core/testing/module_test_base.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

using ::testing::_;

namespace blink {

namespace {

class ModuleScriptTestModulator final : public DummyModulator {
 public:
  explicit ModuleScriptTestModulator(ScriptState* script_state)
      : script_state_(script_state) {}
  ~ModuleScriptTestModulator() override = default;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    DummyModulator::Trace(visitor);
  }

 private:
  ScriptState* GetScriptState() override { return script_state_.Get(); }

  Member<ScriptState> script_state_;
};

class MockCachedMetadataSender : public CachedMetadataSender {
 public:
  MockCachedMetadataSender() = default;

  MOCK_METHOD2(Send, void(CodeCacheHost*, base::span<const uint8_t>));
  bool IsServedFromCacheStorage() override { return false; }
};

ClassicScript* CreateClassicScript(const String& source_text,
                                   CachedMetadataHandler* cache_handler) {
  return ClassicScript::Create(source_text, KURL(), KURL(),
                               ScriptFetchOptions(),
                               ScriptSourceLocationType::kInternal,
                               SanitizeScriptErrors::kSanitize, cache_handler);
}

static const int kScriptRepeatLength = 500;

}  // namespace

class ModuleScriptTest : public ::testing::Test, public ModuleTestBase {
 protected:
  static String LargeSourceText(const char* suffix = nullptr) {
    StringBuilder builder;
    // Returns a sufficiently long script that is eligible for V8 code cache.
    builder.Append(String("window.foo = "));
    for (int i = 0; i < kScriptRepeatLength; ++i) {
      builder.Append(String("1 + "));
    }
    builder.Append(String("0;"));
    if (suffix)
      builder.Append(String(suffix));
    return builder.ToString();
  }

  static JSModuleScript* CreateJSModuleScript(
      Modulator* modulator,
      const String& source_text,
      CachedMetadataHandler* cache_handler) {
    ModuleScriptCreationParams params(
        KURL("https://fox.url/script.js"), KURL("https://fox.url/"),
        ScriptSourceLocationType::kInline, ModuleType::kJavaScript,
        ParkableString(source_text.Impl()->IsolatedCopy()), cache_handler,
        network::mojom::ReferrerPolicy::kDefault);
    return JSModuleScript::Create(params, modulator, ScriptFetchOptions());
  }

  static ValueWrapperSyntheticModuleScript*
  CreateValueWrapperSyntheticModuleScript(Modulator* modulator,
                                          v8::Local<v8::Value> local_value) {
    return ValueWrapperSyntheticModuleScript::CreateWithDefaultExport(
        local_value, modulator, KURL("https://fox.url/script.js"),
        KURL("https://fox.url/"), ScriptFetchOptions());
  }

  // Tests |window.foo| is set correctly, and reset |window.foo| for the next
  // test.
  static void TestFoo(V8TestingScope& scope) {
    v8::Local<v8::Value> value =
        ClassicScript::CreateUnspecifiedScript("window.foo")
            ->RunScriptAndReturnValue(&scope.GetWindow())
            .GetSuccessValueOrEmpty();
    EXPECT_TRUE(value->IsNumber());
    EXPECT_EQ(kScriptRepeatLength,
              value->NumberValue(scope.GetContext()).ToChecked());

    ClassicScript::CreateUnspecifiedScript("window.foo = undefined;")
        ->RunScript(&scope.GetWindow());
  }

  // Accessors for ModuleScript private members.
  static V8CodeCache::ProduceCacheOptions GetProduceCacheOptions(
      const JSModuleScript* module_script) {
    return module_script->produce_cache_data_->GetProduceCacheOptions();
  }

  static bool HandlerCachedMetadataWasDiscarded(
      CachedMetadataHandler* cache_handler) {
    auto* handler = static_cast<ScriptCachedMetadataHandler*>(cache_handler);
    if (!handler)
      return false;
    return handler->cached_metadata_discarded_;
  }

  void SetUp() override { ModuleTestBase::SetUp(); }

  void TearDown() override {
    feature_list_.Reset();
    ModuleTestBase::TearDown();
  }

  test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

// Test expectations depends on heuristics in V8CodeCache and therefore these
// tests should be updated if necessary when V8CodeCache is modified.
TEST_F(ModuleScriptTest, V8CodeCache) {
  using Checkpoint = testing::StrictMock<testing::MockFunction<void(int)>>;

  V8TestingScope scope;
  Modulator* modulator =
      MakeGarbageCollected<ModuleScriptTestModulator>(scope.GetScriptState());
  Modulator::SetModulator(scope.GetScriptState(), modulator);

  auto sender = std::make_unique<MockCachedMetadataSender>();
  MockCachedMetadataSender* sender_ptr = sender.get();
  CachedMetadataHandler* cache_handler =
      MakeGarbageCollected<ScriptCachedMetadataHandler>(UTF8Encoding(),
                                                        std::move(sender));
  const uint32_t kTimeStampTag = V8CodeCache::TagForTimeStamp(cache_handler);
  const uint32_t kCodeTag = V8CodeCache::TagForCodeCache(cache_handler);

  // Tests the main code path: simply produce and consume code cache.
  for (int nth_load = 0; nth_load < 3; ++nth_load) {
    // Compile a module script.
    JSModuleScript* module_script =
        CreateJSModuleScript(modulator, LargeSourceText(), cache_handler);
    ASSERT_TRUE(module_script);

    // Check that the module script is instantiated/evaluated correctly.
    ASSERT_TRUE(ModuleRecord::Instantiate(scope.GetScriptState(),
                                          module_script->V8Module(),
                                          module_script->SourceUrl())
                    .IsEmpty());
    ASSERT_EQ(module_script
                  ->RunScriptOnScriptStateAndReturnValue(scope.GetScriptState())
                  .GetResultType(),
              ScriptEvaluationResult::ResultType::kSuccess);
    TestFoo(scope);

    Checkpoint checkpoint;
    ::testing::InSequence s;

    switch (nth_load) {
      case 0:
        // For the first time, the cache handler doesn't contain any data, and
        // we'll set timestamp in ProduceCache() below.
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kCodeTag));
        EXPECT_EQ(V8CodeCache::ProduceCacheOptions::kSetTimeStamp,
                  GetProduceCacheOptions(module_script));
        EXPECT_CALL(*sender_ptr, Send(_, _));
        break;

      case 1:
        // For the second time, as timestamp is already set, we'll produce code
        // cache in ProduceCache() below.
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kCodeTag));
        EXPECT_EQ(V8CodeCache::ProduceCacheOptions::kProduceCodeCache,
                  GetProduceCacheOptions(module_script));
        EXPECT_CALL(*sender_ptr, Send(_, _));
        break;

      case 2:
        // For the third time, the code cache is already there and we've
        // consumed the code cache and won't do anything in ProduceCache().
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kCodeTag));
        EXPECT_EQ(V8CodeCache::ProduceCacheOptions::kNoProduceCache,
                  GetProduceCacheOptions(module_script));
        break;
    }

    EXPECT_CALL(checkpoint, Call(3));

    module_script->ProduceCache();

    checkpoint.Call(3);

    switch (nth_load) {
      case 0:
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kCodeTag));
        break;

      case 1:
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kCodeTag));
        break;

      case 2:
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kCodeTag));
        break;
    }
  }

  // Tests anything wrong doesn't occur when module script code cache is
  // consumed by a classic script.

  Checkpoint checkpoint;
  ::testing::InSequence s;

  // As code cache is mismatched and rejected by V8, the CachedMetadata are
  // cleared and notified to Platform.
  EXPECT_CALL(*sender_ptr, Send(_, _));
  EXPECT_CALL(checkpoint, Call(4));

  CreateClassicScript(LargeSourceText(), cache_handler)
      ->RunScript(&scope.GetWindow());

  checkpoint.Call(4);

  TestFoo(scope);

  // The CachedMetadata are cleared.
  EXPECT_FALSE(cache_handler->GetCachedMetadata(kTimeStampTag));
  EXPECT_FALSE(cache_handler->GetCachedMetadata(kCodeTag));
}

TEST_F(ModuleScriptTest, ValueWrapperSyntheticModuleScript) {
  V8TestingScope scope;
  v8::Local<v8::Value> local_value(v8::Number::New(scope.GetIsolate(), 1234));
  Modulator* modulator =
      MakeGarbageCollected<ModuleScriptTestModulator>(scope.GetScriptState());
  ValueWrapperSyntheticModuleScript* module_script =
      CreateValueWrapperSyntheticModuleScript(modulator, local_value);
  ASSERT_FALSE(module_script->V8Module().IsEmpty());
}

TEST_F(ModuleScriptTest, V8CodeCacheWithHashChecking) {
  using Checkpoint = testing::StrictMock<testing::MockFunction<void(int)>>;

  V8TestingScope scope;
  Modulator* modulator =
      MakeGarbageCollected<ModuleScriptTestModulator>(scope.GetScriptState());
  Modulator::SetModulator(scope.GetScriptState(), modulator);

  auto sender = std::make_unique<MockCachedMetadataSender>();
  MockCachedMetadataSender* sender_ptr = sender.get();
  ScriptCachedMetadataHandlerWithHashing* cache_handler =
      MakeGarbageCollected<ScriptCachedMetadataHandlerWithHashing>(
          UTF8Encoding(), std::move(sender));
  const uint32_t kTimeStampTag = V8CodeCache::TagForTimeStamp(cache_handler);
  const uint32_t kCodeTag = V8CodeCache::TagForCodeCache(cache_handler);

  // Six loads:
  // 0: cold, should produce timestamp
  // 1: source text changed, should produce timestamp
  // 2: warm, should produce code cache
  // 3: source text changed again, should produce timestamp
  // 4: warm, should produce code cache
  // 5: hot, should consume code cache
  for (int nth_load = 0; nth_load < 6; ++nth_load) {
    // Running the module script immediately clears the code cache contents if
    // it detects a hash mismatch. Thus, some checks must occur before it is
    // called.
    switch (nth_load) {
      case 1:
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kCodeTag));
        EXPECT_CALL(*sender_ptr, Send(_, _));
        break;

      case 3:
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kCodeTag));
        EXPECT_CALL(*sender_ptr, Send(_, _));
        break;
    }

    // Compile a module script.
    String source =
        LargeSourceText((nth_load == 1 || nth_load == 2) ? " " : nullptr);
    cache_handler->ResetForTesting();
    JSModuleScript* module_script =
        CreateJSModuleScript(modulator, source, cache_handler);
    ASSERT_TRUE(module_script);

    // Check that the module script is instantiated/evaluated correctly.
    ASSERT_TRUE(ModuleRecord::Instantiate(scope.GetScriptState(),
                                          module_script->V8Module(),
                                          module_script->SourceUrl())
                    .IsEmpty());
    ASSERT_EQ(module_script
                  ->RunScriptOnScriptStateAndReturnValue(scope.GetScriptState())
                  .GetResultType(),
              ScriptEvaluationResult::ResultType::kSuccess);
    TestFoo(scope);

    Checkpoint checkpoint;
    ::testing::InSequence s;

    switch (nth_load) {
      case 0:
        // For the first time, the cache handler doesn't contain any data, and
        // we'll set timestamp in ProduceCache() below.
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kCodeTag));
        EXPECT_EQ(V8CodeCache::ProduceCacheOptions::kSetTimeStamp,
                  GetProduceCacheOptions(module_script));
        EXPECT_CALL(*sender_ptr, Send(_, _));
        break;

      case 1:
        // For the second time, the timestamp has been cleared and will be
        // replaced by another timestamp because the content didn't match.
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kCodeTag));
        EXPECT_EQ(V8CodeCache::ProduceCacheOptions::kSetTimeStamp,
                  GetProduceCacheOptions(module_script));
        EXPECT_CALL(*sender_ptr, Send(_, _));
        break;

      case 2:
        // For the third time, as timestamp is already set, we'll produce code
        // cache in ProduceCache() below.
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kCodeTag));
        EXPECT_EQ(V8CodeCache::ProduceCacheOptions::kProduceCodeCache,
                  GetProduceCacheOptions(module_script));
        EXPECT_CALL(*sender_ptr, Send(_, _));
        break;

      case 3:
        // For the fourth time, the code cache has been cleared and will get
        // replaced with a timestamp in ProduceCache() due to a content
        // mismatch.
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kCodeTag));
        EXPECT_EQ(V8CodeCache::ProduceCacheOptions::kSetTimeStamp,
                  GetProduceCacheOptions(module_script));
        EXPECT_CALL(*sender_ptr, Send(_, _));
        break;

      case 4:
        // For the fifth time, as timestamp is already set, we'll produce code
        // cache in ProduceCache() below.
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kCodeTag));
        EXPECT_EQ(V8CodeCache::ProduceCacheOptions::kProduceCodeCache,
                  GetProduceCacheOptions(module_script));
        EXPECT_CALL(*sender_ptr, Send(_, _));
        break;

      case 5:
        // For the sixth time, the code cache is already there and we've
        // consumed the code cache and won't do anything in ProduceCache().
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kCodeTag));
        EXPECT_EQ(V8CodeCache::ProduceCacheOptions::kNoProduceCache,
                  GetProduceCacheOptions(module_script));
        break;
    }

    EXPECT_CALL(checkpoint, Call(3));

    module_script->ProduceCache();

    checkpoint.Call(3);

    switch (nth_load) {
      case 0:
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kCodeTag));
        break;

      case 1:
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kCodeTag));
        break;

      case 2:
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kCodeTag));
        break;

      case 3:
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kCodeTag));
        break;

      case 4:
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kCodeTag));
        break;

      case 5:
        EXPECT_FALSE(cache_handler->GetCachedMetadata(kTimeStampTag));
        EXPECT_TRUE(cache_handler->GetCachedMetadata(kCodeTag));
        break;
    }
  }
}

}  // namespace blink
