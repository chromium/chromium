// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"

#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"

namespace blink {

// On debug builds, Oilpan contains checks that will fail if a persistent handle
// is destroyed on the wrong thread.
TEST(SerializedScriptValueThreadedTest,
     SafeDestructionIfSendingThreadKeepsAlive) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  // Start a worker.
  WorkerReportingProxy proxy;
  WorkerThreadForTest worker_thread(proxy);
  worker_thread.StartWithSourceCode(scope.GetWindow().GetSecurityOrigin(),
                                    "/* no worker script */");

  // Create a serialized script value that contains transferred array buffer
  // contents.
  DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(1, 1);
  Transferables transferables;
  transferables.array_buffers.push_back(array_buffer);
  SerializedScriptValue::SerializeOptions options;
  options.transferables = &transferables;
  scoped_refptr<SerializedScriptValue> serialized =
      SerializedScriptValue::Serialize(
          scope.GetIsolate(),
          ToV8Traits<DOMArrayBuffer>::ToV8(scope.GetScriptState(),
                                           array_buffer),
          options, ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(serialized);
  EXPECT_TRUE(array_buffer->IsDetached());

  // Deserialize the serialized value on the worker.
  // Intentionally keep a reference on this thread while this occurs.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      worker_thread.GetWorkerBackingThread().BackingThread().GetTaskRunner();

  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          [](WorkerThread* worker_thread,
             scoped_refptr<SerializedScriptValue> serialized) {
            WorkerOrWorkletScriptController* script =
                worker_thread->GlobalScope()->ScriptController();
            EXPECT_TRUE(script->IsContextInitialized());
            ScriptState::Scope worker_scope(script->GetScriptState());
            SerializedScriptValue::Unpack(serialized)
                ->Deserialize(worker_thread->GetIsolate());

            // Make sure this thread's references in the Oilpan heap are dropped
            // before the main thread continues.
            ThreadState::Current()->CollectAllGarbageForTesting();
          },
          CrossThreadUnretained(&worker_thread), serialized));

  // Wait for a subsequent task on the worker to finish, to ensure that the
  // references held by the task are dropped.
  base::WaitableEvent done;
  PostCrossThreadTask(*task_runner, FROM_HERE,
                      CrossThreadBindOnce(&base::WaitableEvent::Signal,
                                          CrossThreadUnretained(&done)));
  done.Wait();

  // Now destroy the value on the main thread.
  EXPECT_TRUE(serialized->HasOneRef());
  serialized = nullptr;

  // Finally, shut down the worker thread.
  worker_thread.Terminate();
  worker_thread.WaitForShutdownForTesting();
}

}  // namespace blink
