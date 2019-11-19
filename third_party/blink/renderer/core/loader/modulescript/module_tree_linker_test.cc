// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/module_tree_linker.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/boxed_v8_module.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_tree_linker_registry.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/testing/dummy_modulator.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class TestModuleTreeClient final : public ModuleTreeClient {
 public:
  TestModuleTreeClient() = default;

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(module_script_);
    ModuleTreeClient::Trace(visitor);
  }

  void NotifyModuleTreeLoadFinished(ModuleScript* module_script) override {
    was_notify_finished_ = true;
    module_script_ = module_script;
  }

  bool WasNotifyFinished() const { return was_notify_finished_; }
  ModuleScript* GetModuleScript() { return module_script_; }

 private:
  bool was_notify_finished_ = false;
  Member<ModuleScript> module_script_;
};

}  // namespace

class ModuleTreeLinkerTestModulator final : public DummyModulator {
 public:
  ModuleTreeLinkerTestModulator(ScriptState* script_state)
      : script_state_(script_state) {}
  ~ModuleTreeLinkerTestModulator() override = default;

  void Trace(blink::Visitor*) override;

  enum class ResolveResult { kFailure, kSuccess };

  // Resolve last |Modulator::FetchSingle()| call.
  ModuleScript* ResolveSingleModuleScriptFetch(
      const KURL& url,
      const Vector<String>& dependency_module_specifiers,
      bool parse_error = false) {
    ScriptState::Scope scope(script_state_);

    StringBuilder source_text;
    for (const auto& specifier : dependency_module_specifiers) {
      source_text.Append("import '");
      source_text.Append(specifier);
      source_text.Append("';\n");
    }
    source_text.Append("export default 'grapes';");

    v8::Local<v8::Module> module_record = ModuleRecord::Compile(
        script_state_->GetIsolate(), source_text.ToString(), url, url,
        ScriptFetchOptions(), TextPosition::MinimumPosition(),
        ASSERT_NO_EXCEPTION);
    auto* module_script =
        JSModuleScript::CreateForTest(this, module_record, url);

    auto result_map = module_map_.insert(url, module_script);
    EXPECT_TRUE(result_map.is_new_entry);

    if (parse_error) {
      v8::Local<v8::Value> error = V8ThrowException::CreateError(
          script_state_->GetIsolate(), "Parse failure.");
      module_script->SetParseErrorAndClearRecord(
          ScriptValue(script_state_->GetIsolate(), error));
    }

    EXPECT_TRUE(pending_clients_.Contains(url));
    pending_clients_.Take(url)->NotifyModuleLoadFinished(module_script);

    return module_script;
  }

  void ResolveDependentTreeFetch(const KURL& url, ResolveResult result) {
    ResolveSingleModuleScriptFetch(url, Vector<String>(),
                                   result == ResolveResult::kFailure);
  }

  void SetInstantiateShouldFail(bool b) { instantiate_should_fail_ = b; }

  bool HasInstantiated(ModuleScript* module_script) const {
    v8::Isolate* isolate = script_state_->GetIsolate();
    v8::HandleScope scope(isolate);

    return instantiated_records_.Contains(MakeGarbageCollected<BoxedV8Module>(
        isolate, module_script->V8Module()));
  }

 private:
  // Implements Modulator:

  ScriptState* GetScriptState() override { return script_state_; }

  KURL ResolveModuleSpecifier(const String& module_request,
                              const KURL& base_url,
                              String* failure_reason) final {
    return KURL(base_url, module_request);
  }
  void ClearIsAcquiringImportMaps() final {}

  void FetchSingle(const ModuleScriptFetchRequest& request,
                   ResourceFetcher*,
                   ModuleGraphLevel,
                   ModuleScriptCustomFetchType,
                   SingleModuleClient* client) override {
    EXPECT_FALSE(pending_clients_.Contains(request.Url()));
    pending_clients_.Set(request.Url(), client);
  }

  ModuleScript* GetFetchedModuleScript(const KURL& url) override {
    const auto& it = module_map_.find(url);
    if (it == module_map_.end())
      return nullptr;

    return it->value;
  }

  ScriptValue InstantiateModule(v8::Local<v8::Module> record,
                                const KURL& source_url) override {
    if (instantiate_should_fail_) {
      ScriptState::Scope scope(script_state_);
      v8::Local<v8::Value> error = V8ThrowException::CreateError(
          script_state_->GetIsolate(), "Instantiation failure.");
      return ScriptValue(script_state_->GetIsolate(), error);
    }
    instantiated_records_.insert(MakeGarbageCollected<BoxedV8Module>(
        script_state_->GetIsolate(), record));
    return ScriptValue();
  }

  Vector<ModuleRequest> ModuleRequestsFromModuleRecord(
      v8::Local<v8::Module> module_record) override {
    ScriptState::Scope scope(script_state_);
    Vector<String> specifiers =
        ModuleRecord::ModuleRequests(script_state_, module_record);
    Vector<TextPosition> positions =
        ModuleRecord::ModuleRequestPositions(script_state_, module_record);
    DCHECK_EQ(specifiers.size(), positions.size());
    Vector<ModuleRequest> requests;
    requests.ReserveInitialCapacity(specifiers.size());
    for (wtf_size_t i = 0; i < specifiers.size(); ++i) {
      requests.emplace_back(specifiers[i], positions[i]);
    }
    return requests;
  }

  Member<ScriptState> script_state_;
  HeapHashMap<KURL, Member<SingleModuleClient>> pending_clients_;
  HeapHashMap<KURL, Member<ModuleScript>> module_map_;
  HeapHashSet<Member<BoxedV8Module>> instantiated_records_;
  bool instantiate_should_fail_ = false;
};

void ModuleTreeLinkerTestModulator::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(pending_clients_);
  visitor->Trace(module_map_);
  visitor->Trace(instantiated_records_);
  DummyModulator::Trace(visitor);
}

class ModuleTreeLinkerTest : public PageTestBase {
  DISALLOW_COPY_AND_ASSIGN(ModuleTreeLinkerTest);

 public:
  ModuleTreeLinkerTest() = default;
  void SetUp() override;

  ModuleTreeLinkerTestModulator* GetModulator() { return modulator_.Get(); }

 protected:
  Persistent<ModuleTreeLinkerTestModulator> modulator_;
};

void ModuleTreeLinkerTest::SetUp() {
  PageTestBase::SetUp(IntSize(500, 500));
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  modulator_ =
      MakeGarbageCollected<ModuleTreeLinkerTestModulator>(script_state);
}

TEST_F(ModuleTreeLinkerTest, FetchTreeNoDeps) {
  auto* registry = MakeGarbageCollected<ModuleTreeLinkerRegistry>();

  KURL url("http://example.com/root.js");
  TestModuleTreeClient* client = MakeGarbageCollected<TestModuleTreeClient>();
  ModuleTreeLinker::Fetch(url, GetDocument().Fetcher(),
                          mojom::RequestContextType::SCRIPT,
                          ScriptFetchOptions(), GetModulator(),
                          ModuleScriptCustomFetchType::kNone, registry, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  GetModulator()->ResolveSingleModuleScriptFetch(url, {});
  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(GetModulator()->HasInstantiated(client->GetModuleScript()));
}

TEST_F(ModuleTreeLinkerTest, FetchTreeInstantiationFailure) {
  GetModulator()->SetInstantiateShouldFail(true);

  ModuleTreeLinkerRegistry* registry =
      MakeGarbageCollected<ModuleTreeLinkerRegistry>();

  KURL url("http://example.com/root.js");
  TestModuleTreeClient* client = MakeGarbageCollected<TestModuleTreeClient>();
  ModuleTreeLinker::Fetch(url, GetDocument().Fetcher(),
                          mojom::RequestContextType::SCRIPT,
                          ScriptFetchOptions(), GetModulator(),
                          ModuleScriptCustomFetchType::kNone, registry, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  GetModulator()->ResolveSingleModuleScriptFetch(url, {});

  // Modulator::InstantiateModule() fails here, as
  // we SetInstantiateShouldFail(true) earlier.

  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(client->GetModuleScript()->HasErrorToRethrow())
      << "Expected errored module script but got "
      << *client->GetModuleScript();
}

TEST_F(ModuleTreeLinkerTest, FetchTreeWithSingleDependency) {
  ModuleTreeLinkerRegistry* registry =
      MakeGarbageCollected<ModuleTreeLinkerRegistry>();

  KURL url("http://example.com/root.js");
  TestModuleTreeClient* client = MakeGarbageCollected<TestModuleTreeClient>();
  ModuleTreeLinker::Fetch(url, GetDocument().Fetcher(),
                          mojom::RequestContextType::SCRIPT,
                          ScriptFetchOptions(), GetModulator(),
                          ModuleScriptCustomFetchType::kNone, registry, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  GetModulator()->ResolveSingleModuleScriptFetch(url, {"./dep1.js"});
  EXPECT_FALSE(client->WasNotifyFinished());

  KURL url_dep1("http://example.com/dep1.js");

  GetModulator()->ResolveDependentTreeFetch(
      url_dep1, ModuleTreeLinkerTestModulator::ResolveResult::kSuccess);
  EXPECT_TRUE(client->WasNotifyFinished());

  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(GetModulator()->HasInstantiated(client->GetModuleScript()));
}

TEST_F(ModuleTreeLinkerTest, FetchTreeWith3Deps) {
  ModuleTreeLinkerRegistry* registry =
      MakeGarbageCollected<ModuleTreeLinkerRegistry>();

  KURL url("http://example.com/root.js");
  TestModuleTreeClient* client = MakeGarbageCollected<TestModuleTreeClient>();
  ModuleTreeLinker::Fetch(url, GetDocument().Fetcher(),
                          mojom::RequestContextType::SCRIPT,
                          ScriptFetchOptions(), GetModulator(),
                          ModuleScriptCustomFetchType::kNone, registry, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  GetModulator()->ResolveSingleModuleScriptFetch(
      url, {"./dep1.js", "./dep2.js", "./dep3.js"});
  EXPECT_FALSE(client->WasNotifyFinished());

  Vector<KURL> url_deps;
  for (int i = 1; i <= 3; ++i) {
    StringBuilder url_dep_str;
    url_dep_str.Append("http://example.com/dep");
    url_dep_str.AppendNumber(i);
    url_dep_str.Append(".js");

    KURL url_dep(url_dep_str.ToString());
    url_deps.push_back(url_dep);
  }

  for (const auto& url_dep : url_deps) {
    EXPECT_FALSE(client->WasNotifyFinished());
    GetModulator()->ResolveDependentTreeFetch(
        url_dep, ModuleTreeLinkerTestModulator::ResolveResult::kSuccess);
  }

  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(GetModulator()->HasInstantiated(client->GetModuleScript()));
}

TEST_F(ModuleTreeLinkerTest, FetchTreeWith3Deps1Fail) {
  ModuleTreeLinkerRegistry* registry =
      MakeGarbageCollected<ModuleTreeLinkerRegistry>();

  KURL url("http://example.com/root.js");
  TestModuleTreeClient* client = MakeGarbageCollected<TestModuleTreeClient>();
  ModuleTreeLinker::Fetch(url, GetDocument().Fetcher(),
                          mojom::RequestContextType::SCRIPT,
                          ScriptFetchOptions(), GetModulator(),
                          ModuleScriptCustomFetchType::kNone, registry, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  GetModulator()->ResolveSingleModuleScriptFetch(
      url, {"./dep1.js", "./dep2.js", "./dep3.js"});
  EXPECT_FALSE(client->WasNotifyFinished());

  Vector<KURL> url_deps;
  for (int i = 1; i <= 3; ++i) {
    StringBuilder url_dep_str;
    url_dep_str.Append("http://example.com/dep");
    url_dep_str.AppendNumber(i);
    url_dep_str.Append(".js");

    KURL url_dep(url_dep_str.ToString());
    url_deps.push_back(url_dep);
  }

  for (const auto& url_dep : url_deps) {
    SCOPED_TRACE(url_dep.GetString());
  }

  auto url_dep = url_deps.back();
  url_deps.pop_back();
  GetModulator()->ResolveDependentTreeFetch(
      url_dep, ModuleTreeLinkerTestModulator::ResolveResult::kSuccess);
  EXPECT_FALSE(client->WasNotifyFinished());
  url_dep = url_deps.back();
  url_deps.pop_back();
  GetModulator()->ResolveDependentTreeFetch(
      url_dep, ModuleTreeLinkerTestModulator::ResolveResult::kFailure);

  // TODO(kouhei): This may not hold once we implement early failure reporting.
  EXPECT_FALSE(client->WasNotifyFinished());

  // Check below doesn't crash.
  url_dep = url_deps.back();
  url_deps.pop_back();
  GetModulator()->ResolveDependentTreeFetch(
      url_dep, ModuleTreeLinkerTestModulator::ResolveResult::kSuccess);
  EXPECT_TRUE(url_deps.IsEmpty());

  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_FALSE(client->GetModuleScript()->HasParseError());
  EXPECT_TRUE(client->GetModuleScript()->HasErrorToRethrow());
}

TEST_F(ModuleTreeLinkerTest, FetchDependencyTree) {
  ModuleTreeLinkerRegistry* registry =
      MakeGarbageCollected<ModuleTreeLinkerRegistry>();

  KURL url("http://example.com/depth1.js");
  TestModuleTreeClient* client = MakeGarbageCollected<TestModuleTreeClient>();
  ModuleTreeLinker::Fetch(url, GetDocument().Fetcher(),
                          mojom::RequestContextType::SCRIPT,
                          ScriptFetchOptions(), GetModulator(),
                          ModuleScriptCustomFetchType::kNone, registry, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  GetModulator()->ResolveSingleModuleScriptFetch(url, {"./depth2.js"});

  KURL url_dep2("http://example.com/depth2.js");

  GetModulator()->ResolveDependentTreeFetch(
      url_dep2, ModuleTreeLinkerTestModulator::ResolveResult::kSuccess);

  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(GetModulator()->HasInstantiated(client->GetModuleScript()));
}

TEST_F(ModuleTreeLinkerTest, FetchDependencyOfCyclicGraph) {
  ModuleTreeLinkerRegistry* registry =
      MakeGarbageCollected<ModuleTreeLinkerRegistry>();

  KURL url("http://example.com/a.js");
  TestModuleTreeClient* client = MakeGarbageCollected<TestModuleTreeClient>();
  ModuleTreeLinker::Fetch(url, GetDocument().Fetcher(),
                          mojom::RequestContextType::SCRIPT,
                          ScriptFetchOptions(), GetModulator(),
                          ModuleScriptCustomFetchType::kNone, registry, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  GetModulator()->ResolveSingleModuleScriptFetch(url, {"./a.js"});

  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(GetModulator()->HasInstantiated(client->GetModuleScript()));
}

}  // namespace blink
