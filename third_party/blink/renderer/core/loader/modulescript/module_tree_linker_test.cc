// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/module_tree_linker.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/boxed_v8_module.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_tree_linker_registry.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/testing/module_test_base.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class TestModuleTreeClient final : public ModuleTreeClient {
 public:
  TestModuleTreeClient() = default;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(module_script_);
    ModuleTreeClient::Trace(visitor);
  }

  void NotifyModuleTreeLoadFinished(ModuleScript* module_script) override {
    was_notify_finished_ = true;
    module_script_ = module_script;
  }

  bool WasNotifyFinished() const { return was_notify_finished_; }
  ModuleScript* GetModuleScript() { return module_script_.Get(); }

 private:
  bool was_notify_finished_ = false;
  Member<ModuleScript> module_script_;
};

class SimModuleRequest : public SimRequestBase {
 public:
  explicit SimModuleRequest(KURL url)
      : SimRequestBase(std::move(url),
                       "text/javascript",
                       /* start_immediately=*/false) {}

  void CompleteWithImports(const Vector<String>& specifiers) {
    StringBuilder source_text;
    for (const auto& specifier : specifiers) {
      source_text.Append("import '");
      source_text.Append(specifier);
      source_text.Append("';\n");
    }
    source_text.Append("export default 'grapes';");

    Complete(source_text.ToString());
  }
};

}  // namespace

class ModuleTreeLinkerTest : public SimTest {
 public:
  Modulator* GetModulator() {
    return Modulator::From(ToScriptStateForMainWorld(MainFrame().GetFrame()));
  }

  bool HasInstantiated(ModuleScript* module_script) {
    if (!module_script)
      return false;
    ScriptState::Scope script_scope(GetModulator()->GetScriptState());
    return (module_script->V8Module()->GetStatus() ==
            v8::Module::kInstantiated);
  }
};

TEST_F(ModuleTreeLinkerTest, FetchTreeNoDeps) {
  SimModuleRequest sim_module(KURL("http://example.com/root.js"));
  TestModuleTreeClient* client = MakeGarbageCollected<TestModuleTreeClient>();
  GetModulator()->FetchTree(
      sim_module.GetURL(), ModuleType::kJavaScript, GetDocument().Fetcher(),
      mojom::blink::RequestContextType::SCRIPT,
      network::mojom::RequestDestination::kScript, ScriptFetchOptions(),
      ModuleScriptCustomFetchType::kNone, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  sim_module.Complete(R"(export default 'grapes';)");
  test::RunPendingTasks();

  EXPECT_TRUE(client->WasNotifyFinished());

  ModuleScript* module_script = client->GetModuleScript();
  ASSERT_TRUE(module_script);
  EXPECT_TRUE(HasInstantiated(module_script));
}

TEST_F(ModuleTreeLinkerTest, FetchTreeInstantiationFailure) {
  SimModuleRequest sim_module(KURL("http://example.com/root.js"));

  TestModuleTreeClient* client = MakeGarbageCollected<TestModuleTreeClient>();
  GetModulator()->FetchTree(
      sim_module.GetURL(), ModuleType::kJavaScript, GetDocument().Fetcher(),
      mojom::blink::RequestContextType::SCRIPT,
      network::mojom::RequestDestination::kScript, ScriptFetchOptions(),
      ModuleScriptCustomFetchType::kNone, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  sim_module.Complete(R"(
    import _self_should_fail from 'http://example.com/root.js';
  )");
  test::RunPendingTasks();

  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(client->GetModuleScript()->HasErrorToRethrow())
      << "Expected errored module script but got "
      << *client->GetModuleScript();
}

TEST_F(ModuleTreeLinkerTest, FetchTreeWithSingleDependency) {
  SimModuleRequest sim_module(KURL("http://example.com/root.js"));
  SimModuleRequest sim_module_dep(KURL("http://example.com/dep1.js"));

  TestModuleTreeClient* client = MakeGarbageCollected<TestModuleTreeClient>();
  GetModulator()->FetchTree(
      sim_module.GetURL(), ModuleType::kJavaScript, GetDocument().Fetcher(),
      mojom::blink::RequestContextType::SCRIPT,
      network::mojom::RequestDestination::kScript, ScriptFetchOptions(),
      ModuleScriptCustomFetchType::kNone, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  sim_module.CompleteWithImports({"./dep1.js"});
  test::RunPendingTasks();

  EXPECT_FALSE(client->WasNotifyFinished());

  sim_module_dep.CompleteWithImports({});
  test::RunPendingTasks();

  EXPECT_TRUE(client->WasNotifyFinished());
  ModuleScript* module_script = client->GetModuleScript();
  ASSERT_TRUE(module_script);
  EXPECT_TRUE(HasInstantiated(module_script));
}

TEST_F(ModuleTreeLinkerTest, FetchTreeWith3Deps) {
  SimModuleRequest sim_module(KURL("http://example.com/root.js"));

  TestModuleTreeClient* client = MakeGarbageCollected<TestModuleTreeClient>();
  GetModulator()->FetchTree(
      sim_module.GetURL(), ModuleType::kJavaScript, GetDocument().Fetcher(),
      mojom::blink::RequestContextType::SCRIPT,
      network::mojom::RequestDestination::kScript, ScriptFetchOptions(),
      ModuleScriptCustomFetchType::kNone, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  Vector<std::unique_ptr<SimModuleRequest>> sim_module_deps;
  for (int i = 1; i <= 3; ++i) {
    StringBuilder url_dep_str;
    url_dep_str.Append("http://example.com/dep");
    url_dep_str.AppendNumber(i);
    url_dep_str.Append(".js");

    KURL url_dep(url_dep_str.ToString());
    sim_module_deps.push_back(std::make_unique<SimModuleRequest>(url_dep));
  }

  sim_module.CompleteWithImports({"./dep1.js", "./dep2.js", "./dep3.js"});
  test::RunPendingTasks();

  for (const auto& sim_module_dep : sim_module_deps) {
    EXPECT_FALSE(client->WasNotifyFinished());
    sim_module_dep->CompleteWithImports({});
    test::RunPendingTasks();
  }

  EXPECT_TRUE(client->WasNotifyFinished());
  ModuleScript* module_script = client->GetModuleScript();
  ASSERT_TRUE(module_script);
  EXPECT_TRUE(HasInstantiated(module_script));
}

TEST_F(ModuleTreeLinkerTest, FetchTreeWith3Deps1Fail) {
  SimModuleRequest sim_module(KURL("http://example.com/root.js"));

  TestModuleTreeClient* client = MakeGarbageCollected<TestModuleTreeClient>();
  GetModulator()->FetchTree(
      sim_module.GetURL(), ModuleType::kJavaScript, GetDocument().Fetcher(),
      mojom::blink::RequestContextType::SCRIPT,
      network::mojom::RequestDestination::kScript, ScriptFetchOptions(),
      ModuleScriptCustomFetchType::kNone, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  Vector<std::unique_ptr<SimModuleRequest>> sim_module_deps;
  for (int i = 1; i <= 3; ++i) {
    StringBuilder url_dep_str;
    url_dep_str.Append("http://example.com/dep");
    url_dep_str.AppendNumber(i);
    url_dep_str.Append(".js");

    KURL url_dep(url_dep_str.ToString());
    sim_module_deps.push_back(std::make_unique<SimModuleRequest>(url_dep));
  }

  sim_module.CompleteWithImports({"./dep1.js", "./dep2.js", "./dep3.js"});
  test::RunPendingTasks();

  for (int i = 0; i < 3; ++i) {
    const auto& sim_module_dep = sim_module_deps[i];

    EXPECT_FALSE(client->WasNotifyFinished());
    if (i == 1) {
      // Complete the request with un-parsable JavaScript fragment.
      sim_module_dep->Complete("%!#$@#$@#$@");
    } else {
      sim_module_dep->CompleteWithImports({});
    }

    test::RunPendingTasks();
  }

  EXPECT_TRUE(client->WasNotifyFinished());
  ModuleScript* module_script = client->GetModuleScript();
  ASSERT_TRUE(module_script);
  EXPECT_FALSE(HasInstantiated(module_script));
  EXPECT_FALSE(module_script->HasParseError());
  EXPECT_TRUE(module_script->HasErrorToRethrow());
}

TEST_F(ModuleTreeLinkerTest, FetchDependencyOfCyclicGraph) {
  SimModuleRequest sim_module(KURL("http://example.com/a.js"));

  TestModuleTreeClient* client = MakeGarbageCollected<TestModuleTreeClient>();
  GetModulator()->FetchTree(
      sim_module.GetURL(), ModuleType::kJavaScript, GetDocument().Fetcher(),
      mojom::blink::RequestContextType::SCRIPT,
      network::mojom::RequestDestination::kScript, ScriptFetchOptions(),
      ModuleScriptCustomFetchType::kNone, client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  sim_module.CompleteWithImports({"./a.js"});
  test::RunPendingTasks();

  EXPECT_TRUE(client->WasNotifyFinished());
  ModuleScript* module_script = client->GetModuleScript();
  ASSERT_TRUE(module_script);
  EXPECT_TRUE(HasInstantiated(module_script));
  EXPECT_FALSE(module_script->HasParseError());
}

}  // namespace blink
