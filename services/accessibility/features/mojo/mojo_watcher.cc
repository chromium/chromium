// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/mojo/mojo_watcher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/handle.h"
#include "gin/public/wrapper_info.h"
#include "mojo/public/c/system/trap.h"
#include "mojo/public/c/system/types.h"
#include "services/accessibility/features/mojo/mojo_watch_callback.h"
#include "services/accessibility/features/registered_wrappable.h"
#include "services/accessibility/features/v8_manager.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"

namespace ax {

// Holds a Persistent to a MojoWatcher V8 object so that the MojoWatcher doesn't
// get garbage collected before it has finished.
class MojoWatcher::Persistent
    : public base::RefCountedDeleteOnSequence<Persistent> {
 public:
  Persistent(scoped_refptr<base::SequencedTaskRunner> task_runner,
             v8::Isolate* isolate,
             v8::Local<v8::Value> mojo_watcher_obj,
             MojoWatcher* mojo_watcher);
  Persistent(const Persistent&) = delete;
  Persistent& operator=(const Persistent&) = delete;

  // Called by Mojo when there is a new MojoTrapEvent.
  // Extracts a Persistent pointer from the event and uses
  // it to RunReadyCallback.
  // May be called from any thread.
  static void OnHandleReady(const MojoTrapEvent*);

  // Called by the owning MojoWatcher to request RunReadyCallback
  // be scheduled in the task queue. Ensures that the MojoWatcher
  // and `this` object cannot go out of scope until RunReadyCallback
  // is completed by adding a ref before posting the task, and removing
  // the ref when running the task.
  void ScheduleRunReadyCallback(MojoResult result);

  // Called by the owning MojowWatcher when it observes that the Isolate
  // will be destroyed. This allows this Persistent to release the Isolate
  // it was holding
  void OnIsolateWillDestroy();

 private:
  friend class base::RefCountedDeleteOnSequence<Persistent>;
  friend class base::DeleteHelper<Persistent>;
  ~Persistent();

  void RemoveRefAndRunReadyCallback(MojoResult result);
  void RunReadyCallback(MojoResult result);

  // A v8::Persistent around the MojoWatcher's V8/gin object.
  v8::Persistent<v8::Value> persistent_;

  // Unowned. This MojoWatcher owns the Persistent.
  raw_ptr<MojoWatcher> mojo_watcher_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

//                                        //
// MojoWatcher::Persistent implementation //
//                                        //

MojoWatcher::Persistent::Persistent(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    v8::Isolate* isolate,
    v8::Local<v8::Value> mojo_watcher_obj,
    MojoWatcher* mojo_watcher)
    : base::RefCountedDeleteOnSequence<MojoWatcher::Persistent>(
          std::move(task_runner)),
      persistent_(isolate, mojo_watcher_obj),
      mojo_watcher_(mojo_watcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

MojoWatcher::Persistent::~Persistent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoWatcher::Persistent::OnIsolateWillDestroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo_watcher_ = nullptr;
}

// static
void MojoWatcher::Persistent::OnHandleReady(const MojoTrapEvent* event) {
  // It is safe to assume this MojoWatcher::Persistent still exists, because
  // we keep it alive until we've dispatched MOJO_RESULT_CANCELLED from here to
  // RunReadyCallback, and that is always the last notification we'll dispatch.
  auto* wrap =
      reinterpret_cast<MojoWatcher::Persistent*>(event->trigger_context);

  // It is safe to use base::Unretained because MojoWatcher, which is in the
  // v8::Persistent member var, has a ref to `this` Persistent that is not
  // cleared until MOJO_RESULT_CANCELLED.
  wrap->owning_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MojoWatcher::Persistent::RunReadyCallback,
                                base::Unretained(wrap), event->result));
}

void MojoWatcher::Persistent::ScheduleRunReadyCallback(MojoResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddRef();
  // Safe to use base::Unretained because this has another
  // ref that won't be cleared until this method executes.
  owning_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&MojoWatcher::Persistent::RemoveRefAndRunReadyCallback,
                     base::Unretained(this), result));
}

void MojoWatcher::Persistent::RemoveRefAndRunReadyCallback(MojoResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Release();
  RunReadyCallback(result);
}

void MojoWatcher::Persistent::RunReadyCallback(MojoResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!mojo_watcher_) {
    return;
  }
  mojo_watcher_->RunReadyCallback(result);
}

//                            //
// MojoWatcher implementation //
//                            //

// static
gin::WrapperInfo MojoWatcher::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
v8::Local<v8::Object> MojoWatcher::Create(
    v8::Local<v8::Context> context,
    mojo::Handle handle,
    bool readable,
    bool writable,
    bool peer_closed,
    std::unique_ptr<MojoWatchCallback> callback) {
  v8::Isolate* isolate = context->GetIsolate();
  CHECK(isolate);

  // Start observing.
  MojoWatcher* watcher = new MojoWatcher(context, std::move(callback));
  MojoResult result =
      watcher->Watch(handle, readable, writable, peer_closed, isolate);

  gin::Handle<MojoWatcher> mojo_watcher_handle =
      gin::CreateHandle(isolate, watcher);
  v8::Local<v8::Object> object = mojo_watcher_handle.ToV8()
                                     ->ToObject(isolate->GetCurrentContext())
                                     .ToLocalChecked();

  // TODO(alokp): Consider raising an exception.
  // Current clients expect to receive the initial error returned by MojoWatch
  // via watch callback.
  //
  // Note that the usage of the Persistent is intentional so that the initial
  // error is guaranteed to be reported to the client in case where the given
  // handle is invalid and garbage collection happens before the callback
  // is scheduled. This will work even if the MojoWatcher's `persistent_wrap_`
  // was not constructed.
  if (result != MOJO_RESULT_OK) {
    v8::Global<v8::Object> global(isolate, object);

    // base::Unretained is safe because MojoWatcher cannot be destroyed until
    // the persistent `global` of itself is reset.
    watcher->task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MojoWatcher::CallCallbackFromTaskRunner,
                       base::Unretained(watcher), result, std::move(global)));
  }

  return object;
}

MojoWatcher::~MojoWatcher() {
  // It shouldn't be possible to destruct the MojoWatcher while its Persistent
  // exists.
  DCHECK(!persistent_wrap_);
}

void MojoWatcher::OnIsolateWillDestroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (persistent_wrap_) {
    // May be null if this is during shutdown.
    persistent_wrap_->OnIsolateWillDestroy();
  }
  Cancel(nullptr);
  // Stop observing the V8 isolate. Cancel() will clear `persistent_wrap_` and
  // allow garbage collection to clear this instance.
  StopObserving();

  // Reset the callback not to keep the Isolate in MojoWatchCallback.
  callback_ = nullptr;
}

gin::ObjectTemplateBuilder MojoWatcher::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<MojoWatcher>::GetObjectTemplateBuilder(isolate)
      .SetMethod("cancel", &MojoWatcher::Cancel);
}

void MojoWatcher::Cancel(gin::Arguments* arguments) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MojoResult result;
  if (!trap_handle_.is_valid()) {
    result = MOJO_RESULT_INVALID_ARGUMENT;
  } else {
    trap_handle_.reset();
    result = MOJO_RESULT_OK;
  }
  if (arguments) {
    arguments->Return(result);
  }

  // We don't need to clear the persistent_wrap_ here because closing
  // trap_handle_ will end up with a MOJO_RESULT_CANCELLED coming back in
  // OnHandleReady.
}

MojoWatcher::MojoWatcher(v8::Local<v8::Context> context,
                         std::unique_ptr<MojoWatchCallback> callback)
    : RegisteredWrappable(context), callback_(std::move(callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Get the current thread (which is the isolate thread).
  task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

MojoResult MojoWatcher::Watch(mojo::Handle handle,
                              bool readable,
                              bool writable,
                              bool peer_closed,
                              v8::Isolate* isolate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ::MojoHandleSignals signals = MOJO_HANDLE_SIGNAL_NONE;
  if (readable) {
    signals |= MOJO_HANDLE_SIGNAL_READABLE;
  }
  if (writable) {
    signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
  }
  if (peer_closed) {
    signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  }

  scoped_refptr<MojoWatcher::Persistent> persistent_wrap(
      new MojoWatcher::Persistent(task_runner_, isolate,
                                  GetWrapper(isolate).ToLocalChecked(), this));

  MojoResult result =
      mojo::CreateTrap(&MojoWatcher::Persistent::OnHandleReady, &trap_handle_);
  DCHECK_EQ(MOJO_RESULT_OK, result);

  result = MojoAddTrigger(trap_handle_.get().value(), handle.value(), signals,
                          MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                          reinterpret_cast<uintptr_t>(persistent_wrap.get()),
                          nullptr);
  if (result != MOJO_RESULT_OK) {
    return result;
  }

  // If MojoAddTrigger succeeded above, we need this object to stay alive at
  // least until OnHandleReady is invoked with MOJO_RESULT_CANCELLED, which
  // signals the final invocation by the trap.
  // Constructs a keep-alive by adding a reference to the Persistent which has
  // the v8::Persistent to `this`.
  persistent_wrap_ = persistent_wrap;

  handle_ = handle;

  MojoResult ready_result;
  result = Arm(&ready_result);
  if (result == MOJO_RESULT_OK) {
    return result;
  }

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // We couldn't arm the watcher because the handle is already ready to
    // notify with `ready_result`. Post a notification manually.
    // Safe to use base::Unretained because the persistent_wrap_ adds another
    // ref that won't be cleared until this method executes.
    CHECK(persistent_wrap_);
    persistent_wrap->ScheduleRunReadyCallback(ready_result);
    return MOJO_RESULT_OK;
  }

  // If MojoAddTrigger succeeds but Arm does not, that means another thread
  // closed the watched handle in between. Treat it like we'd treat
  // MojoAddTrigger trying to watch an invalid handle.
  trap_handle_.reset();
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult MojoWatcher::Arm(MojoResult* ready_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Nothing to do if the watcher is inactive.
  if (!handle_.is_valid()) {
    return MOJO_RESULT_OK;
  }

  uint32_t num_blocking_events = 1;
  MojoTrapEvent blocking_event = {sizeof(blocking_event)};
  MojoResult result = MojoArmTrap(trap_handle_.get().value(), nullptr,
                                  &num_blocking_events, &blocking_event);
  if (result == MOJO_RESULT_OK) {
    return MOJO_RESULT_OK;
  }

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    DCHECK_EQ(1u, num_blocking_events);
    DCHECK_EQ(reinterpret_cast<uintptr_t>(
                  reinterpret_cast<uintptr_t>(persistent_wrap_.get())),
              blocking_event.trigger_context);
    *ready_result = blocking_event.result;
    return result;
  }

  return result;
}

void MojoWatcher::RunReadyCallback(MojoResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result == MOJO_RESULT_CANCELLED) {
    // Last notification.
    handle_ = mojo::Handle();

    // Only dispatch to the callback if this cancellation was implicit due to
    // |handle_| closure. If it was explicit, |trap_handle_| has already been
    // reset.
    if (trap_handle_.is_valid()) {
      trap_handle_.reset();
      CallCallbackWithResult(result);
    }

    // Resetting the Persistent will allow this object to be
    // garbage collected as usual.
    persistent_wrap_.reset();
    return;
  }

  // Ignore callbacks if not watching.
  if (!trap_handle_.is_valid()) {
    return;
  }

  CallCallbackWithResult(result);

  // The user callback may have canceled watching.
  if (!trap_handle_.is_valid()) {
    return;
  }

  // Rearm the watcher so another notification can fire.
  MojoResult ready_result;
  MojoResult arm_result = Arm(&ready_result);
  if (arm_result == MOJO_RESULT_OK) {
    return;
  }

  if (arm_result == MOJO_RESULT_FAILED_PRECONDITION) {
    CHECK(persistent_wrap_);
    persistent_wrap_->ScheduleRunReadyCallback(ready_result);
    return;
  }
}

void MojoWatcher::CallCallbackWithResult(MojoResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callback_) {
    callback_->Call(result);
  }
}

void MojoWatcher::CallCallbackFromTaskRunner(
    MojoResult result,
    v8::Global<v8::Object> self_global) {
  DCHECK(!self_global.IsEmpty());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CallCallbackWithResult(result);
}

}  // namespace ax
