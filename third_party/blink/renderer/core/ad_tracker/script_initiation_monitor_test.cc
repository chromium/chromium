// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/script_initiation_monitor.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

class MockObserver final : public GarbageCollected<MockObserver>,
                           public ScriptInitiationMonitor::Observer {
 public:
  void WillExecuteScript(ExecutionContext& execution_context,
                         V8ScriptId script_id,
                         const String& script_url,
                         LazyStackTrace& stack_trace) override {
    will_execute_script_called_ = true;
    script_id_ = script_id;
    url_ = script_url;
  }
  void DidExecuteScript(V8ScriptId script_id) override {
    did_execute_script_called_ = true;
    script_id_ = script_id;
  }
  void WillCallFunction(ExecutionContext& execution_context,
                        V8ScriptId script_id,
                        bool is_nested,
                        LazyStackTrace& stack_trace) override {
    will_call_function_called_ = true;
    script_id_ = script_id;
    is_nested_ = is_nested;
  }
  void DidCallFunction(V8ScriptId script_id, bool is_nested) override {
    did_call_function_called_ = true;
    script_id_ = script_id;
    is_nested_ = is_nested;
  }
  void DidCreateAsyncTask(probe::AsyncTaskContext* task_context,
                          LazyStackTrace& stack_trace) override {
    did_create_async_task_called_ = true;
  }
  void DidStartAsyncTask(probe::AsyncTaskContext* task_context) override {
    did_start_async_task_called_ = true;
  }
  void DidFinishAsyncTask(probe::AsyncTaskContext* task_context) override {
    did_finish_async_task_called_ = true;
  }
  void DidRegisterDynamicScript(V8ScriptId script_id,
                                LazyStackTrace& stack_trace) override {
    did_register_dynamic_script_called_ = true;
    script_id_ = script_id;
  }
  void Trace(Visitor* visitor) const override {
    ScriptInitiationMonitor::Observer::Trace(visitor);
  }

  bool will_execute_script_called_ = false;
  bool did_execute_script_called_ = false;
  bool did_register_dynamic_script_called_ = false;
  bool will_call_function_called_ = false;
  bool did_call_function_called_ = false;
  bool did_create_async_task_called_ = false;
  bool did_start_async_task_called_ = false;
  bool did_finish_async_task_called_ = false;

  V8ScriptId script_id_;
  String url_;
  bool is_nested_ = false;
};

}  // namespace

TEST(ScriptInitiationMonitorTest, ObserverBroadcast) {
  test::TaskEnvironment task_environment;
  auto page_holder = std::make_unique<DummyPageHolder>();
  LocalFrame& frame = page_holder->GetFrame();
  ScriptInitiationMonitor* monitor = frame.GetScriptInitiationMonitor();
  ASSERT_NE(monitor, nullptr);

  auto* observer = MakeGarbageCollected<MockObserver>();
  monitor->AddObserver(observer);

  V8ScriptId script_id(42);
  KURL url("https://example.com/test.js");

  v8::HandleScope handle_scope(monitor->GetIsolate());
  // Call the probe manually.
  {
    v8::Context::Scope context_scope(
        ToScriptStateForMainWorld(&frame)->GetContext());
    probe::ExecuteScript probe_data(frame.GetDocument()->GetExecutionContext(),
                                    url.GetString(), script_id.value());
    monitor->Will(probe_data);
    EXPECT_TRUE(observer->will_execute_script_called_);
    EXPECT_EQ(observer->script_id_, script_id);
    EXPECT_EQ(observer->url_, url.GetString());

    monitor->Did(probe_data);
    EXPECT_TRUE(observer->did_execute_script_called_);
    EXPECT_EQ(observer->script_id_, script_id);
  }

  // Test deregistration.
  observer->will_execute_script_called_ = false;
  monitor->RemoveObserver(observer);

  {
    v8::Context::Scope context_scope(
        ToScriptStateForMainWorld(&frame)->GetContext());
    probe::ExecuteScript probe_data(frame.GetDocument()->GetExecutionContext(),
                                    url.GetString(), script_id.value());
    monitor->Will(probe_data);
    EXPECT_FALSE(observer->will_execute_script_called_);
  }
}

class ScriptInitiationMonitorSimTest : public SimTest {};

TEST_F(ScriptInitiationMonitorSimTest, LocalRootScoping) {
  SimRequest main_resource("https://example.com/main.html", "text/html");
  SimRequest child_resource("https://example.com/child.html", "text/html");
  LoadURL("https://example.com/main.html");

  main_resource.Complete(R"HTML(
    <body>
      <iframe id="child" src="child.html"></iframe>
    </body>
  )HTML");

  child_resource.Complete(R"HTML(
    <body>Subframe</body>
  )HTML");

  auto* child_element = To<HTMLFrameOwnerElement>(
      GetDocument().getElementById(AtomicString("child")));
  LocalFrame* child_frame = To<LocalFrame>(child_element->ContentFrame());
  ASSERT_NE(child_frame, nullptr);

  // Both should share the same monitor from the local root.
  EXPECT_EQ(GetDocument().GetFrame()->GetScriptInitiationMonitor(),
            child_frame->GetScriptInitiationMonitor());
}

}  // namespace blink
