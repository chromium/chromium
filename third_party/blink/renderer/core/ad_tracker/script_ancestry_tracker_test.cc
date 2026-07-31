// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/script_ancestry_tracker.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class TestScriptAncestryTracker final : public ScriptAncestryTracker {
 public:
  struct ScriptInfo {
    String url;
    V8ScriptId marked_script_id;
  };

  TestScriptAncestryTracker(LocalFrame* frame, ScriptInitiationMonitor* monitor)
      : ScriptAncestryTracker(frame, monitor) {}
  ~TestScriptAncestryTracker() override = default;

  using ScriptAncestryTracker::GetScriptMetadata;
  using ScriptAncestryTracker::RegisterScript;

  // Mark scripts with "marked" in the url and their descendants.
  void OnScriptRegistered(ExecutionContext& execution_context,
                          V8ScriptId script_id,
                          const String& url,
                          std::optional<V8ScriptId> marked_script_id) override {
    bool is_marked =
        url.find("marked") != kNotFound ||
        (marked_script_id.has_value() && IsMarkedScript(*marked_script_id));

    if (is_marked) {
      marked_scripts_.insert(script_id);
    }

    registered_scripts_.insert(
        script_id, ScriptInfo{url, marked_script_id.value_or(V8ScriptId())});

    if (marked_script_id.has_value()) {
      marked_script_was_on_stack_.insert(script_id);
    }
  }

  bool IsMarkedScript(V8ScriptId script_id) const override {
    return marked_scripts_.Contains(script_id);
  }

  const ScriptInfo* GetRegisteredScript(V8ScriptId script_id) const {
    auto it = registered_scripts_.find(script_id);
    return it != registered_scripts_.end() ? &it->value : nullptr;
  }

  bool WasMarkedScriptOnStackDuringCompilation(V8ScriptId script_id) const {
    return marked_script_was_on_stack_.Contains(script_id);
  }

  V8ScriptId FindScriptIdByUrl(const String& url_substring) const {
    for (const auto& [script_id, info] : registered_scripts_) {
      if (info.url.find(url_substring) != kNotFound) {
        return script_id;
      }
    }
    return V8ScriptId();
  }

  // Wait for a script matching `url_substring` to have been registered in
  // `OnScriptRegistered`.
  V8ScriptId WaitAndFindScriptIdByUrl(const String& url_substring) const {
    V8ScriptId found_id;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      found_id = FindScriptIdByUrl(url_substring);
      return found_id != V8ScriptId();
    }));
    return found_id;
  }

  Vector<V8ScriptId> FindAllScriptIdsByUrl(const String& url_substring) const {
    Vector<V8ScriptId> ids;
    for (const auto& [script_id, info] : registered_scripts_) {
      if (info.url.find(url_substring) != kNotFound) {
        ids.push_back(script_id);
      }
    }
    return ids;
  }

  Vector<V8ScriptId> WaitAndFindAllScriptIdsByUrl(
      const String& url_substring,
      size_t expected_min_count) const {
    Vector<V8ScriptId> found_ids;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      found_ids = FindAllScriptIdsByUrl(url_substring);
      return found_ids.size() >= expected_min_count;
    }));
    return found_ids;
  }

 private:
  HashMap<V8ScriptId, ScriptInfo> registered_scripts_;
  HashSet<V8ScriptId> marked_script_was_on_stack_;
  HashSet<V8ScriptId> marked_scripts_;
};

class ScriptAncestryTrackerTest : public SimTest {
 protected:
  ScriptAncestryTrackerTest() = default;

  void WaitForElementAttribute(const AtomicString& id,
                               const QualifiedName& attr) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      Element* el = GetDocument().getElementById(id);
      return el && el->hasAttribute(attr);
    }));
  }

  void SetUp() override {
    SimTest::SetUp();
    main_resource_ = std::make_unique<SimRequest>(
        "https://example.com/test.html", "text/html");

    LoadURL("https://example.com/test.html");
    LocalFrame* frame = GetDocument().GetFrame();
    test_observer_ = MakeGarbageCollected<TestScriptAncestryTracker>(
        frame, frame->GetScriptInitiationMonitor());
    tracker_ = test_observer_.Get();
  }

  std::unique_ptr<SimRequest> main_resource_;
  Persistent<ScriptAncestryTracker> tracker_;
  Persistent<TestScriptAncestryTracker> test_observer_;
};

TEST_F(ScriptAncestryTrackerTest, ClassicExternalScript) {
  SimSubresourceRequest initial_script("https://example.com/initial_script.js",
                                       "text/javascript");
  main_resource_->Complete(
      "<body><script src=\"initial_script.js\"></script></body>");
  test::RunPendingTasks();

  initial_script.Complete("console.log('hello');");
  V8ScriptId script_id =
      test_observer_->WaitAndFindScriptIdByUrl("initial_script.js");
  EXPECT_NE(V8ScriptId(), script_id);
  EXPECT_FALSE(test_observer_->IsMarkedScript(script_id));

  const auto* info = test_observer_->GetRegisteredScript(script_id);
  ASSERT_TRUE(info);
  EXPECT_EQ("https://example.com/initial_script.js", info->url);
  EXPECT_EQ(V8ScriptId(), info->marked_script_id);
}

TEST_F(ScriptAncestryTrackerTest, ClassicExternalMarkedScript) {
  SimSubresourceRequest marked_script("https://example.com/marked_script.js",
                                      "text/javascript");
  main_resource_->Complete(
      "<body><script src=\"marked_script.js\"></script></body>");
  test::RunPendingTasks();

  marked_script.Complete("console.log('hello');");
  V8ScriptId script_id =
      test_observer_->WaitAndFindScriptIdByUrl("marked_script.js");
  EXPECT_NE(V8ScriptId(), script_id);
  EXPECT_TRUE(test_observer_->IsMarkedScript(script_id));
}

TEST_F(ScriptAncestryTrackerTest,
       ClassicExternalMarkedScript_BottomMostScriptSet) {
  SimSubresourceRequest marked_script("https://example.com/marked_script.js",
                                      "text/javascript");
  SimSubresourceRequest child_script("https://example.com/child.js",
                                     "text/javascript");
  main_resource_->Complete(
      "<body><script src=\"marked_script.js\"></script></body>");
  test::RunPendingTasks();

  // Define a marked script that creates a dynamic script.
  marked_script.Complete(
      "var s = document.createElement('script');"
      "s.src = 'child.js';"
      "document.body.appendChild(s);");
  test::RunPendingTasks();

  child_script.Complete("console.log('child');");

  V8ScriptId marked_id =
      test_observer_->WaitAndFindScriptIdByUrl("marked_script.js");
  V8ScriptId child_id = test_observer_->WaitAndFindScriptIdByUrl("child.js");

  EXPECT_NE(V8ScriptId(), marked_id);
  EXPECT_NE(V8ScriptId(), child_id);
  EXPECT_TRUE(test_observer_->IsMarkedScript(marked_id));

  // The child script's parent marked script ID should be set to marked_id
  // because marked_id was set as bottom_most_script_ during WillExecuteScript.
  const auto* child_info = test_observer_->GetRegisteredScript(child_id);
  ASSERT_TRUE(child_info);
  EXPECT_EQ(marked_id, child_info->marked_script_id);
}

TEST_F(ScriptAncestryTrackerTest, TransitiveExternalScript) {
  SimSubresourceRequest initial_script(
      "https://example.com/initial_marked_script.js", "text/javascript");
  SimSubresourceRequest child_script("https://example.com/child_script.js",
                                     "text/javascript");
  main_resource_->Complete(
      "<body><script src=\"initial_marked_script.js\"></script></body>");
  test::RunPendingTasks();

  // The initial script dynamically appends an external script.
  initial_script.Complete(
      "var s = document.createElement('script');"
      "s.src = 'child_script.js';"
      "document.body.appendChild(s);");
  test::RunPendingTasks();

  // Complete child script load so that it compiles and registers.
  child_script.Complete("console.log('child');");
  V8ScriptId initial_id =
      test_observer_->WaitAndFindScriptIdByUrl("initial_marked_script.js");
  V8ScriptId child_id = test_observer_->FindScriptIdByUrl("child_script.js");

  EXPECT_NE(V8ScriptId(), initial_id);
  EXPECT_NE(V8ScriptId(), child_id);
  EXPECT_TRUE(test_observer_->IsMarkedScript(initial_id));
  EXPECT_TRUE(test_observer_->IsMarkedScript(child_id));

  // Verify that the child script's initiating marked script was correctly
  // tracked as the initial script.
  const auto* child_info = test_observer_->GetRegisteredScript(child_id);
  ASSERT_TRUE(child_info);
  EXPECT_EQ(initial_id, child_info->marked_script_id);
}

TEST_F(ScriptAncestryTrackerTest, TransitiveExternalScript_NotMarked) {
  SimSubresourceRequest initial_script("https://example.com/initial_script.js",
                                       "text/javascript");
  SimSubresourceRequest child_script("https://example.com/child_script.js",
                                     "text/javascript");
  main_resource_->Complete(
      "<body><script src=\"initial_script.js\"></script></body>");
  test::RunPendingTasks();

  // The initial script dynamically appends an external script.
  initial_script.Complete(
      "var s = document.createElement('script');"
      "s.src = 'child_script.js';"
      "document.body.appendChild(s);");
  test::RunPendingTasks();

  // Complete child script load so that it compiles and registers.
  child_script.Complete("console.log('child');");
  V8ScriptId initial_id =
      test_observer_->WaitAndFindScriptIdByUrl("initial_script.js");
  V8ScriptId child_id = test_observer_->FindScriptIdByUrl("child_script.js");

  EXPECT_NE(V8ScriptId(), initial_id);
  EXPECT_NE(V8ScriptId(), child_id);
  EXPECT_FALSE(test_observer_->IsMarkedScript(initial_id));
  EXPECT_FALSE(test_observer_->IsMarkedScript(child_id));

  // Since the initiating script was not marked, the child should NOT have a
  // marked script tracked in the ancestry graph.
  const auto* child_info = test_observer_->GetRegisteredScript(child_id);
  ASSERT_TRUE(child_info);
  EXPECT_EQ(V8ScriptId(), child_info->marked_script_id);
}

TEST_F(ScriptAncestryTrackerTest, RegisterScriptDirectly) {
  main_resource_->Complete("<body></body>");
  test::RunPendingTasks();

  V8ScriptId script_id(101);
  V8ScriptId marked_script_id(102);

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = ToV8Context(
      GetDocument().GetFrame(), DOMWrapperWorld::MainWorld(isolate));
  v8::Context::Scope context_scope(context);

  test_observer_->RegisterScript(context, script_id, marked_script_id);

  const auto* data = test_observer_->GetScriptMetadata(script_id);
  ASSERT_TRUE(data);
  EXPECT_EQ(marked_script_id, data->marked_script_id);

  const auto* observer_data = test_observer_->GetRegisteredScript(script_id);
  ASSERT_TRUE(observer_data);
  EXPECT_EQ(marked_script_id, observer_data->marked_script_id);
}

TEST_F(ScriptAncestryTrackerTest, DynamicInlineScriptInsertion) {
  SimSubresourceRequest initial_script(
      "https://example.com/initial_marked_script.js", "text/javascript");
  main_resource_->Complete(
      "<body><script src=\"initial_marked_script.js\"></script></body>");
  test::RunPendingTasks();

  initial_script.Complete(
      "var s = document.createElement('script');"
      "s.text = 'console.log(\"inline\");';"
      "document.body.appendChild(s);");
  V8ScriptId initial_id =
      test_observer_->WaitAndFindScriptIdByUrl("initial_marked_script.js");
  V8ScriptId inline_id = test_observer_->FindScriptIdByUrl("{ id ");

  EXPECT_NE(V8ScriptId(), initial_id);
  EXPECT_NE(V8ScriptId(), inline_id);

  const auto* inline_info = test_observer_->GetRegisteredScript(inline_id);
  ASSERT_TRUE(inline_info);
  EXPECT_EQ(initial_id, inline_info->marked_script_id);
  EXPECT_TRUE(
      test_observer_->WasMarkedScriptOnStackDuringCompilation(inline_id));
}

TEST_F(ScriptAncestryTrackerTest, ModuleScript) {
  SimSubresourceRequest module_script("https://example.com/module_script.js",
                                      "text/javascript");
  main_resource_->Complete(
      "<body><script type=\"module\" "
      "src=\"module_script.js\"></script></body>");
  test::RunPendingTasks();

  module_script.Complete("console.log('module');");
  V8ScriptId script_id =
      test_observer_->WaitAndFindScriptIdByUrl("module_script.js");
  EXPECT_NE(V8ScriptId(), script_id);

  const auto* info = test_observer_->GetRegisteredScript(script_id);
  ASSERT_TRUE(info);
  EXPECT_EQ("https://example.com/module_script.js", info->url);
}

TEST_F(ScriptAncestryTrackerTest, DocumentWriteScript) {
  SimSubresourceRequest initial_script(
      "https://example.com/initial_marked_script.js", "text/javascript");
  main_resource_->Complete(
      "<body><script src=\"initial_marked_script.js\"></script></body>");
  test::RunPendingTasks();

  initial_script.Complete(
      "document.write('<script>console.log(\"write\");</script>');");
  V8ScriptId initial_id =
      test_observer_->WaitAndFindScriptIdByUrl("initial_marked_script.js");
  V8ScriptId write_script_id = test_observer_->FindScriptIdByUrl("{ id ");

  EXPECT_NE(V8ScriptId(), initial_id);
  EXPECT_NE(V8ScriptId(), write_script_id);

  const auto* write_info = test_observer_->GetRegisteredScript(write_script_id);
  ASSERT_TRUE(write_info);
  EXPECT_EQ(initial_id, write_info->marked_script_id);
  EXPECT_TRUE(
      test_observer_->WasMarkedScriptOnStackDuringCompilation(write_script_id));
}

TEST_F(ScriptAncestryTrackerTest, EvalScript) {
  SimSubresourceRequest initial_script(
      "https://example.com/initial_marked_script.js", "text/javascript");
  main_resource_->Complete(
      "<body><script src=\"initial_marked_script.js\"></script></body>");
  test::RunPendingTasks();

  // The initial script calls eval which dynamically appends an inline script.
  initial_script.Complete(
      "eval(\""
      "  var s = document.createElement('script');"
      "  s.text = 'console.log(\\\"eval_inline\\\");';"
      "  document.body.appendChild(s);"
      "\");");
  V8ScriptId initial_id =
      test_observer_->WaitAndFindScriptIdByUrl("initial_marked_script.js");
  V8ScriptId inline_id = test_observer_->FindScriptIdByUrl("{ id ");

  EXPECT_NE(V8ScriptId(), initial_id);
  EXPECT_NE(V8ScriptId(), inline_id);

  // Verify that the dynamically appended script is correctly attributed.
  const auto* info = test_observer_->GetRegisteredScript(inline_id);
  ASSERT_TRUE(info);
  EXPECT_EQ(initial_id, info->marked_script_id);
}

TEST_F(ScriptAncestryTrackerTest, NewFunctionScript) {
  SimSubresourceRequest initial_script(
      "https://example.com/initial_marked_script.js", "text/javascript");
  main_resource_->Complete(
      "<body><script src=\"initial_marked_script.js\"></script></body>");
  test::RunPendingTasks();

  // The initial script creates a new Function which dynamically appends an
  // inline script.
  initial_script.Complete(
      "var f = new Function(\""
      "  var s = document.createElement('script');"
      "  s.text = 'console.log(\\\"new_fn_inline\\\");';"
      "  document.body.appendChild(s);"
      "\");"
      "f();");
  V8ScriptId initial_id =
      test_observer_->WaitAndFindScriptIdByUrl("initial_marked_script.js");
  V8ScriptId inline_id = test_observer_->FindScriptIdByUrl("{ id ");

  EXPECT_NE(V8ScriptId(), initial_id);
  EXPECT_NE(V8ScriptId(), inline_id);

  // Verify that the dynamically appended script is correctly attributed.
  const auto* info = test_observer_->GetRegisteredScript(inline_id);
  ASSERT_TRUE(info);
  EXPECT_EQ(initial_id, info->marked_script_id);
}

TEST_F(ScriptAncestryTrackerTest, JavascriptUrlScript) {
  SimSubresourceRequest initial_script(
      "https://example.com/initial_marked_script.js", "text/javascript");
  main_resource_->Complete(
      "<body><script src=\"initial_marked_script.js\"></script></body>");
  test::RunPendingTasks();

  // The initial script appends an iframe with a javascript: URL that appends an
  // inline script.
  initial_script.Complete(
      "const iframe = document.createElement('iframe');"
      "iframe.src = 'javascript:"
      "  var s = parent.document.createElement(\\\"script\\\");"
      "  s.text = \\\"console.log(\\\\\\\"js_url_inline\\\\\\\");\\\";"
      "  parent.document.body.appendChild(s);"
      "';"
      "document.body.appendChild(iframe);");
  V8ScriptId initial_id =
      test_observer_->WaitAndFindScriptIdByUrl("initial_marked_script.js");
  V8ScriptId inline_id = test_observer_->FindScriptIdByUrl("{ id ");

  EXPECT_NE(V8ScriptId(), initial_id);
  EXPECT_NE(V8ScriptId(), inline_id);

  // Verify that the script compiled during the javascript: execution is
  // correctly attributed.
  const auto* info = test_observer_->GetRegisteredScript(inline_id);
  ASSERT_TRUE(info);
  EXPECT_EQ(initial_id, info->marked_script_id);
}

TEST_F(ScriptAncestryTrackerTest, AttributeScript) {
  SimSubresourceRequest initial_script(
      "https://example.com/initial_marked_script.js", "text/javascript");
  main_resource_->Complete(
      "<body><div id=\"test_div\">Click me</div>"
      "<script src=\"initial_marked_script.js\"></script></body>");
  test::RunPendingTasks();

  // The initial script adds an onclick attribute to the div.
  initial_script.Complete(
      "const div = document.getElementById('test_div');"
      "div.setAttribute('onclick', \""
      "  const s = document.createElement('script');"
      "  s.text = 'console.log(\\\"attr_inline\\\");';"
      "  document.body.appendChild(s);"
      "\");");
  test::RunPendingTasks();

  // Trigger the click event to fire the event handler.
  Element* div = GetDocument().getElementById(AtomicString("test_div"));
  div->DispatchSimulatedClick(nullptr);
  V8ScriptId initial_id =
      test_observer_->WaitAndFindScriptIdByUrl("initial_marked_script.js");
  V8ScriptId inline_id = test_observer_->FindScriptIdByUrl("{ id ");

  EXPECT_NE(V8ScriptId(), initial_id);
  EXPECT_NE(V8ScriptId(), inline_id);

  // Verify that the event handler's dynamically appended script is attributed
  // to the initial script.
  const auto* info = test_observer_->GetRegisteredScript(inline_id);
  ASSERT_TRUE(info);
  EXPECT_EQ(initial_id, info->marked_script_id);
}

TEST_F(ScriptAncestryTrackerTest, PromiseResolveScript) {
  SimSubresourceRequest initial_script(
      "https://example.com/initial_marked_script.js", "text/javascript");
  main_resource_->Complete(
      "<body><script src=\"initial_marked_script.js\"></script></body>");
  test::RunPendingTasks();

  // The initial script appends an inline script inside a Promise microtask.
  initial_script.Complete(
      "Promise.resolve().then(() => {"
      "  const s = document.createElement('script');"
      "  s.text = 'console.log(\"promise_inline\");';"
      "  document.body.appendChild(s);"
      "});");
  V8ScriptId initial_id =
      test_observer_->WaitAndFindScriptIdByUrl("initial_marked_script.js");
  V8ScriptId inline_id = test_observer_->FindScriptIdByUrl("{ id ");

  EXPECT_NE(V8ScriptId(), initial_id);
  EXPECT_NE(V8ScriptId(), inline_id);

  // Verify that the microtask's dynamically appended script is correctly
  // attributed.
  const auto* info = test_observer_->GetRegisteredScript(inline_id);
  ASSERT_TRUE(info);
  EXPECT_EQ(initial_id, info->marked_script_id);
}

TEST_F(ScriptAncestryTrackerTest, SetTimeoutWithStringScript) {
  SimSubresourceRequest initial_script(
      "https://example.com/initial_marked_script.js", "text/javascript");
  main_resource_->Complete(
      "<body><script src=\"initial_marked_script.js\"></script></body>");
  test::RunPendingTasks();

  initial_script.Complete("setTimeout('console.log(\"timeout\");', 0);");
  V8ScriptId initial_id =
      test_observer_->WaitAndFindScriptIdByUrl("initial_marked_script.js");
  V8ScriptId timeout_id = test_observer_->FindScriptIdByUrl("{ id ");

  EXPECT_NE(V8ScriptId(), initial_id);
  EXPECT_NE(V8ScriptId(), timeout_id);

  const auto* timeout_info = test_observer_->GetRegisteredScript(timeout_id);
  ASSERT_TRUE(timeout_info);
  EXPECT_EQ(initial_id, timeout_info->marked_script_id);
}

TEST_F(ScriptAncestryTrackerTest, DuplicateExternalScriptSameMarkedScript) {
  SimSubresourceRequest initial_script(
      "https://example.com/initial_marked_script.js", "text/javascript");
  SimRequest::Params params;
  params.response_http_headers = {{"Cache-Control", "max-age=3600"}};
  SimSubresourceRequest child_script("https://example.com/child_script.js",
                                     "text/javascript", params);
  main_resource_->Complete(
      "<body><script src=\"initial_marked_script.js\"></script></body>");
  test::RunPendingTasks();

  // 1. Define function and append first instance
  initial_script.Complete(
      "window.appendScript = function() {"
      "  var s = document.createElement('script');"
      "  s.src = 'child_script.js';"
      "  document.body.appendChild(s);"
      "};"
      "window.appendScript();");
  test::RunPendingTasks();

  // Complete first load
  child_script.Complete("console.log('child 1');");
  V8ScriptId initial_id =
      test_observer_->WaitAndFindScriptIdByUrl("initial_marked_script.js");
  V8ScriptId child_id1 =
      test_observer_->WaitAndFindScriptIdByUrl("child_script.js");

  EXPECT_NE(V8ScriptId(), initial_id);
  EXPECT_NE(V8ScriptId(), child_id1);

  const auto* child_info1 = test_observer_->GetRegisteredScript(child_id1);
  ASSERT_TRUE(child_info1);
  EXPECT_EQ(initial_id, child_info1->marked_script_id);

  // 2. Call appendScript again. This should hit MemoryCache.
  MainFrame().ExecuteScript(WebScriptSource("window.appendScript();"));
  test::RunPendingTasks();

  Vector<V8ScriptId> child_ids =
      test_observer_->FindAllScriptIdsByUrl("child_script.js");

  EXPECT_GE(child_ids.size(), 1u);

  for (V8ScriptId id : child_ids) {
    const auto* info = test_observer_->GetRegisteredScript(id);
    ASSERT_TRUE(info);
    EXPECT_EQ(initial_id, info->marked_script_id);
  }
}

TEST_F(ScriptAncestryTrackerTest,
       IgnoreMonkeyPatchHeuristic_MonkeypatchedAppendChild) {
  SimSubresourceRequest marked_script("https://example.com/marked_script.js",
                                      "text/javascript");
  SimSubresourceRequest vanilla_script("https://example.com/vanilla_script.js",
                                       "text/javascript");
  SimSubresourceRequest target_script("https://example.com/target_script.js",
                                      "text/javascript");

  main_resource_->Complete(R"HTML(
    <body>
      <script src="marked_script.js"></script>
      <script src="vanilla_script.js"></script>
    </body>
  )HTML");
  test::RunPendingTasks();

  // 1. The marked script monkeypatches Node.prototype.appendChild.
  marked_script.Complete(R"SCRIPT(
    const originalAppendChild = Node.prototype.appendChild;
    Node.prototype.appendChild = function(...args) {
      return originalAppendChild.apply(this, args);
    };
  )SCRIPT");
  test::RunPendingTasks();

  // 2. The vanilla script calls the monkeypatched appendChild to add
  // target_script.js.
  vanilla_script.Complete(R"SCRIPT(
    const script = document.createElement("script");
    script.src = "target_script.js";
    document.body.appendChild(script);
  )SCRIPT");
  test::RunPendingTasks();

  target_script.Complete("console.log('target');");

  V8ScriptId marked_id =
      test_observer_->WaitAndFindScriptIdByUrl("marked_script.js");
  V8ScriptId vanilla_id =
      test_observer_->WaitAndFindScriptIdByUrl("vanilla_script.js");
  V8ScriptId target_id =
      test_observer_->WaitAndFindScriptIdByUrl("target_script.js");

  EXPECT_NE(V8ScriptId(), marked_id);
  EXPECT_NE(V8ScriptId(), vanilla_id);
  EXPECT_NE(V8ScriptId(), target_id);
  EXPECT_TRUE(test_observer_->IsMarkedScript(marked_id));
  EXPECT_FALSE(test_observer_->IsMarkedScript(vanilla_id));

  // Thanks to kNodeAppendChild monkeypatch protection in DidCreateAsyncTask,
  // target_script should NOT be attributed to marked_script.js.
  const auto* target_info = test_observer_->GetRegisteredScript(target_id);
  ASSERT_TRUE(target_info);
  EXPECT_NE(marked_id, target_info->marked_script_id);
  EXPECT_FALSE(test_observer_->IsMarkedScript(target_id));
}

}  // namespace blink
