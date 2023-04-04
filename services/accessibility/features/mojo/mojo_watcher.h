// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_MOJO_MOJO_WATCHER_H_
#define SERVICES_ACCESSIBILITY_FEATURES_MOJO_MOJO_WATCHER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/trap.h"
#include "services/accessibility/features/bindings_isolate_holder.h"
#include "services/accessibility/features/registered_wrappable.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-persistent-handle.h"

namespace gin {
class Arguments;
}

namespace ax {
class MojoWatchCallback;

// Provides MojoWatcher object to the Accessibility Service's V8 Javascript.
// This class is a parallel to blink::MojoWatcher, which does the same for
// any blink renderer.
class MojoWatcher : public gin::Wrappable<MojoWatcher>,
                    public RegisteredWrappable {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static v8::Local<v8::Object> Create(
      v8::Local<v8::Context> context,
      mojo::Handle handle,
      bool readable,
      bool writable,
      bool peer_closed,
      std::unique_ptr<MojoWatchCallback> callback);

  ~MojoWatcher() override;
  MojoWatcher(const MojoWatcher&) = delete;
  MojoWatcher& operator=(const MojoWatcher&) = delete;

  // RegisteredWrappable:
  void OnIsolateWillDestroy() override;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  //
  // Methods exposed to Javascript.
  // Note: gin::Wrappable's bound methods need to be public.
  //

  // Stops watching a pipe.
  // See third_party/blink/renderer/core/mojo/mojo_watcher.idl.
  void Cancel(gin::Arguments* arguments);

  //
  // End of methods exposed to Javascript.
  //

 private:
  MojoWatcher(v8::Local<v8::Context> context,
              std::unique_ptr<MojoWatchCallback> callback);

  MojoResult Watch(mojo::Handle handle,
                   bool readable,
                   bool writable,
                   bool peer_closed,
                   v8::Isolate* isolate);

  MojoResult Arm(MojoResult* ready_result);

  void RunReadyCallback(MojoResult result);

  void CallCallbackWithResult(MojoResult result);

  // Bound as a posted task from the task runner.
  // `self_global` is an unused v8::Persistent to this object, and allows this
  // object to be kept alive by the V8 garbage collector until it goes
  // out of scope. Passing it here allows the class to avoid garbage collection
  // until this method is called from a base::OnceCallback where it was bound.
  void CallCallbackFromTaskRunner(MojoResult result,
                                  v8::Global<v8::Object> self_global);

  // Keep a Persistent to the v8 object that is represented by this
  // C++ object; thus |this| can't be destructed before it finishes
  // calling the callback with MOJO_RESULT_CANCELLED.
  class Persistent;
  scoped_refptr<MojoWatcher::Persistent> persistent_wrap_;

  // The task runner upon which to run Javascript.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The callback to inform Javascript of a MojoResult.
  std::unique_ptr<MojoWatchCallback> callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::ScopedTrapHandle trap_handle_;

  mojo::Handle handle_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MojoWatcher> weak_ptr_factory_{this};
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_MOJO_MOJO_WATCHER_H_
