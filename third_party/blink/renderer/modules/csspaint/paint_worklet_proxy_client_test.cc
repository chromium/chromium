// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_worklet_proxy_client.h"

#include <memory>
#include <utility>

#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_paint_worklet_input.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope.h"
#include "third_party/blink/renderer/modules/worklet/worklet_thread_test_common.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"

namespace blink {

// We inject a fake task runner in multiple tests, to avoid actually posting
// tasks cross-thread whilst still being able to know if they have been posted.
class FakeTaskRunner : public base::SingleThreadTaskRunner {
 public:
  FakeTaskRunner() : task_posted_(false) {}

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    task_posted_ = true;
    return true;
  }
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    task_posted_ = true;
    return true;
  }
  bool RunsTasksInCurrentSequence() const override { return true; }

  bool task_posted_;

 protected:
  ~FakeTaskRunner() override {}
};

class PaintWorkletProxyClientTest : public RenderingTest {
 public:
  PaintWorkletProxyClientTest() = default;

  void SetUp() override {
    RenderingTest::SetUp();
    paint_worklet_ =
        MakeGarbageCollected<PaintWorklet>(*GetFrame().DomWindow());
    dispatcher_ = std::make_unique<PaintWorkletPaintDispatcher>();
    fake_compositor_thread_runner_ = base::MakeRefCounted<FakeTaskRunner>();
    proxy_client_ = MakeGarbageCollected<PaintWorkletProxyClient>(
        1, paint_worklet_, GetFrame().GetTaskRunner(TaskType::kInternalDefault),
        dispatcher_->GetWeakPtr(), fake_compositor_thread_runner_);
    reporting_proxy_ = std::make_unique<WorkerReportingProxy>();
  }

  void AddGlobalScopeOnWorkletThread(WorkerThread* worker_thread,
                                     PaintWorkletProxyClient* proxy_client,
                                     base::WaitableEvent* waitable_event) {
    // The natural flow for PaintWorkletGlobalScope is to be registered with the
    // proxy client during its first registerPaint call. Rather than circumvent
    // this with a specialised AddGlobalScopeForTesting method, we just use the
    // standard flow.
    ClassicScript::CreateUnspecifiedScript(
        "registerPaint('add_global_scope', class { paint() { } });")
        ->RunScriptOnScriptState(
            worker_thread->GlobalScope()->ScriptController()->GetScriptState());
    waitable_event->Signal();
  }

  using TestCallback = void (*)(WorkerThread*,
                                PaintWorkletProxyClient*,
                                base::WaitableEvent*);

  void RunMultipleGlobalScopeTestsOnWorklet(TestCallback callback) {
    // PaintWorklet is stateless, and this is enforced via having multiple
    // global scopes (which are switched between). To mimic the real world,
    // create multiple WorkerThread for this. Note that the underlying thread
    // may be shared even though they are unique WorkerThread instances!
    Vector<std::unique_ptr<WorkerThread>> worklet_threads;
    for (wtf_size_t i = 0; i < PaintWorklet::kNumGlobalScopesPerThread; i++) {
      worklet_threads.push_back(CreateThreadAndProvidePaintWorkletProxyClient(
          &GetDocument(), reporting_proxy_.get(), proxy_client_));
    }

    // Add the global scopes. This must happen on the worklet thread.
    for (wtf_size_t i = 0; i < PaintWorklet::kNumGlobalScopesPerThread; i++) {
      base::WaitableEvent waitable_event;
      PostCrossThreadTask(
          *worklet_threads[i]->GetTaskRunner(TaskType::kInternalTest),
          FROM_HERE,
          CrossThreadBindOnce(
              &PaintWorkletProxyClientTest::AddGlobalScopeOnWorkletThread,
              CrossThreadUnretained(this),
              CrossThreadUnretained(worklet_threads[i].get()),
              CrossThreadPersistent<PaintWorkletProxyClient>(proxy_client_),
              CrossThreadUnretained(&waitable_event)));
      waitable_event.Wait();
    }

    // Now let the test actually run. We only run the test on the first worklet
    // thread currently; this suffices since they share the proxy.
    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *worklet_threads[0]->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(
            callback, CrossThreadUnretained(worklet_threads[0].get()),
            CrossThreadPersistent<PaintWorkletProxyClient>(proxy_client_),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();

    // And finally clean up.
    for (wtf_size_t i = 0; i < PaintWorklet::kNumGlobalScopesPerThread; i++) {
      worklet_threads[i]->Terminate();
      worklet_threads[i]->WaitForShutdownForTesting();
    }
  }

  std::unique_ptr<PaintWorkletPaintDispatcher> dispatcher_;
  Persistent<PaintWorklet> paint_worklet_;
  scoped_refptr<FakeTaskRunner> fake_compositor_thread_runner_;
  Persistent<PaintWorkletProxyClient> proxy_client_;
  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
};

TEST_F(PaintWorkletProxyClientTest, PaintWorkletProxyClientConstruction) {
  PaintWorkletProxyClient* proxy_client =
      MakeGarbageCollected<PaintWorkletProxyClient>(
          1, nullptr, GetFrame().GetTaskRunner(TaskType::kInternalDefault),
          nullptr, nullptr);
  EXPECT_EQ(proxy_client->worklet_id_, 1);
  EXPECT_EQ(proxy_client->paint_dispatcher_, nullptr);

  auto dispatcher = std::make_unique<PaintWorkletPaintDispatcher>();

  proxy_client = MakeGarbageCollected<PaintWorkletProxyClient>(
      1, nullptr, GetFrame().GetTaskRunner(TaskType::kInternalDefault),
      dispatcher->GetWeakPtr(), nullptr);
  EXPECT_EQ(proxy_client->worklet_id_, 1);
  EXPECT_NE(proxy_client->paint_dispatcher_, nullptr);
}

void RunAddGlobalScopesTestOnWorklet(
    WorkerThread* thread,
    PaintWorkletProxyClient* proxy_client,
    scoped_refptr<FakeTaskRunner> compositor_task_runner,
    base::WaitableEvent* waitable_event) {
  // For this test, we cheat and reuse the same global scope object from a
  // single WorkerThread. In real code these would be different global scopes.

  // First, add all but one of the global scopes. The proxy client should not
  // yet register itself.
  for (size_t i = 0; i < PaintWorklet::kNumGlobalScopesPerThread - 1; i++) {
    proxy_client->AddGlobalScope(To<WorkletGlobalScope>(thread->GlobalScope()));
  }

  EXPECT_EQ(proxy_client->GetGlobalScopesForTesting().size(),
            PaintWorklet::kNumGlobalScopesPerThread - 1);
  EXPECT_FALSE(compositor_task_runner->task_posted_);

  // Now add the final global scope. This should trigger the registration.
  proxy_client->AddGlobalScope(To<WorkletGlobalScope>(thread->GlobalScope()));
  EXPECT_EQ(proxy_client->GetGlobalScopesForTesting().size(),
            PaintWorklet::kNumGlobalScopesPerThread);
  EXPECT_TRUE(compositor_task_runner->task_posted_);

  waitable_event->Signal();
}

TEST_F(PaintWorkletProxyClientTest, AddGlobalScopes) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  // Global scopes must be created on worker threads.
  std::unique_ptr<WorkerThread> worklet_thread =
      CreateThreadAndProvidePaintWorkletProxyClient(
          &GetDocument(), reporting_proxy_.get(), proxy_client_);

  EXPECT_TRUE(proxy_client_->GetGlobalScopesForTesting().empty());

  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *worklet_thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(
          &RunAddGlobalScopesTestOnWorklet,
          CrossThreadUnretained(worklet_thread.get()),
          CrossThreadPersistent<PaintWorkletProxyClient>(proxy_client_),
          fake_compositor_thread_runner_,
          CrossThreadUnretained(&waitable_event)));
  waitable_event.Wait();

  worklet_thread->Terminate();
  worklet_thread->WaitForShutdownForTesting();
}

void RunPaintTestOnWorklet(WorkerThread* thread,
                           PaintWorkletProxyClient* proxy_client,
                           base::WaitableEvent* waitable_event) {
  // Assert that all global scopes have been registered. Note that we don't
  // use ASSERT_EQ here as that would crash the worklet thread and the test
  // would timeout rather than fail.
  EXPECT_EQ(proxy_client->GetGlobalScopesForTesting().size(),
            PaintWorklet::kNumGlobalScopesPerThread);

  // Register the painter on all global scopes.
  for (const auto& global_scope : proxy_client->GetGlobalScopesForTesting()) {
    ClassicScript::CreateUnspecifiedScript(
        "registerPaint('foo', class { paint() { } });")
        ->RunScriptOnScriptState(
            global_scope->ScriptController()->GetScriptState());
  }

  PaintWorkletStylePropertyMap::CrossThreadData data;
  Vector<std::unique_ptr<CrossThreadStyleValue>> input_arguments;
  std::vector<cc::PaintWorkletInput::PropertyKey> property_keys;
  scoped_refptr<CSSPaintWorkletInput> input =
      base::MakeRefCounted<CSSPaintWorkletInput>(
          "foo", gfx::SizeF(100, 100), 1.0f, 1, std::move(data),
          std::move(input_arguments), std::move(property_keys));
  PaintRecord record = proxy_client->Paint(input.get(), {});
  EXPECT_FALSE(record.empty());

  waitable_event->Signal();
}

TEST_F(PaintWorkletProxyClientTest, Paint) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  RunMultipleGlobalScopeTestsOnWorklet(&RunPaintTestOnWorklet);
}

void RunDefinitionsMustBeCompatibleTestOnWorklet(
    WorkerThread* thread,
    PaintWorkletProxyClient* proxy_client,
    base::WaitableEvent* waitable_event) {
  // Assert that all global scopes have been registered. Note that we don't
  // use ASSERT_EQ here as that would crash the worklet thread and the test
  // would timeout rather than fail.
  EXPECT_EQ(proxy_client->GetGlobalScopesForTesting().size(),
            PaintWorklet::kNumGlobalScopesPerThread);

  // This test doesn't make sense if there's only one global scope!
  EXPECT_GT(PaintWorklet::kNumGlobalScopesPerThread, 1u);

  const Vector<CrossThreadPersistent<PaintWorkletGlobalScope>>& global_scopes =
      proxy_client->GetGlobalScopesForTesting();

  // Things that can be different: alpha different, native properties
  // different, custom properties different, input type args different.
  const HashMap<String, std::unique_ptr<DocumentPaintDefinition>>&
      document_definition_map = proxy_client->DocumentDefinitionMapForTesting();

  // Differing native properties.
  ClassicScript::CreateUnspecifiedScript(R"JS(registerPaint('test1', class {
        static get inputProperties() { return ['border-image', 'color']; }
        paint() { }
      });)JS")
      ->RunScriptOnScriptState(
          global_scopes[0]->ScriptController()->GetScriptState());
  EXPECT_NE(document_definition_map.at("test1"), nullptr);
  ClassicScript::CreateUnspecifiedScript(R"JS(registerPaint('test1', class {
        static get inputProperties() { return ['left']; }
        paint() { }
      });)JS")
      ->RunScriptOnScriptState(
          global_scopes[1]->ScriptController()->GetScriptState());
  EXPECT_EQ(document_definition_map.at("test1"), nullptr);

  // Differing custom properties.
  ClassicScript::CreateUnspecifiedScript(R"JS(registerPaint('test2', class {
        static get inputProperties() { return ['--foo', '--bar']; }
        paint() { }
      });)JS")
      ->RunScriptOnScriptState(
          global_scopes[0]->ScriptController()->GetScriptState());
  EXPECT_NE(document_definition_map.at("test2"), nullptr);
  ClassicScript::CreateUnspecifiedScript(R"JS(registerPaint('test2', class {
        static get inputProperties() { return ['--zoinks']; }
        paint() { }
      });)JS")
      ->RunScriptOnScriptState(
          global_scopes[1]->ScriptController()->GetScriptState());
  EXPECT_EQ(document_definition_map.at("test2"), nullptr);

  // Differing alpha values. The default is 'true'.
  ClassicScript::CreateUnspecifiedScript(
      "registerPaint('test3', class { paint() { } });")
      ->RunScriptOnScriptState(
          global_scopes[0]->ScriptController()->GetScriptState());
  EXPECT_NE(document_definition_map.at("test3"), nullptr);
  ClassicScript::CreateUnspecifiedScript(R"JS(registerPaint('test3', class {
        static get contextOptions() { return {alpha: false}; }
        paint() { }
      });)JS")
      ->RunScriptOnScriptState(
          global_scopes[1]->ScriptController()->GetScriptState());
  EXPECT_EQ(document_definition_map.at("test3"), nullptr);

  waitable_event->Signal();
}

TEST_F(PaintWorkletProxyClientTest, DefinitionsMustBeCompatible) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  RunMultipleGlobalScopeTestsOnWorklet(
      &RunDefinitionsMustBeCompatibleTestOnWorklet);
}

namespace {
// Calling registerPaint can cause the PaintWorkletProxyClient to post back from
// the worklet thread to the main thread. This is safe in the general case,
// since the task will just queue up to run after the test has finished, but
// the following tests want to know whether or not the task has posted; this
// class provides that information.
class ScopedFakeMainThreadTaskRunner {
 public:
  ScopedFakeMainThreadTaskRunner(PaintWorkletProxyClient* proxy_client)
      : proxy_client_(proxy_client), fake_task_runner_(new FakeTaskRunner) {
    original_task_runner_ = proxy_client->MainThreadTaskRunnerForTesting();
    proxy_client_->SetMainThreadTaskRunnerForTesting(fake_task_runner_);
  }

  ~ScopedFakeMainThreadTaskRunner() {
    proxy_client_->SetMainThreadTaskRunnerForTesting(original_task_runner_);
  }

  void ResetTaskHasBeenPosted() { fake_task_runner_->task_posted_ = false; }
  bool TaskHasBeenPosted() const { return fake_task_runner_->task_posted_; }

 private:
  // The PaintWorkletProxyClient is held on the main test thread, but we are
  // constructed on the worklet thread so we have to hold the client reference
  // in a CrossThreadPersistent.
  CrossThreadPersistent<PaintWorkletProxyClient> proxy_client_;
  scoped_refptr<FakeTaskRunner> fake_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> original_task_runner_;
};
}  // namespace

void RunAllDefinitionsMustBeRegisteredBeforePostingTestOnWorklet(
    WorkerThread* thread,
    PaintWorkletProxyClient* proxy_client,
    base::WaitableEvent* waitable_event) {
  ScopedFakeMainThreadTaskRunner fake_runner(proxy_client);

  // Assert that all global scopes have been registered. Note that we don't
  // use ASSERT_EQ here as that would crash the worklet thread and the test
  // would timeout rather than fail.
  EXPECT_EQ(proxy_client->GetGlobalScopesForTesting().size(),
            PaintWorklet::kNumGlobalScopesPerThread);

  // Register a new paint function on all but one global scope. They should not
  // end up posting a task to the PaintWorklet.
  const Vector<CrossThreadPersistent<PaintWorkletGlobalScope>>& global_scopes =
      proxy_client->GetGlobalScopesForTesting();
  for (wtf_size_t i = 0; i < global_scopes.size() - 1; i++) {
    ClassicScript::CreateUnspecifiedScript(
        "registerPaint('foo', class { paint() { } });")
        ->RunScriptOnScriptState(
            global_scopes[i]->ScriptController()->GetScriptState());
    EXPECT_FALSE(fake_runner.TaskHasBeenPosted());
  }

  // Now register the final one; the task should then be posted.
  ClassicScript::CreateUnspecifiedScript(
      "registerPaint('foo', class { paint() { } });")
      ->RunScriptOnScriptState(
          global_scopes.back()->ScriptController()->GetScriptState());
  EXPECT_TRUE(fake_runner.TaskHasBeenPosted());

  waitable_event->Signal();
}

TEST_F(PaintWorkletProxyClientTest,
       AllDefinitionsMustBeRegisteredBeforePosting) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  RunMultipleGlobalScopeTestsOnWorklet(
      &RunAllDefinitionsMustBeRegisteredBeforePostingTestOnWorklet);
}

}  // namespace blink
