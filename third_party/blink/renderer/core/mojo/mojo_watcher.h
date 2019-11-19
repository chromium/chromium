// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MOJO_MOJO_WATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MOJO_MOJO_WATCHER_H_

#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/trap.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExecutionContext;
class MojoHandleSignals;
class V8MojoWatchCallback;

class MojoWatcher final : public ScriptWrappable,
                          public ActiveScriptWrappable<MojoWatcher>,
                          public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MojoWatcher);

 public:
  static MojoWatcher* Create(mojo::Handle,
                             const MojoHandleSignals*,
                             V8MojoWatchCallback*,
                             ExecutionContext*);

  MojoWatcher(ExecutionContext*, V8MojoWatchCallback*);
  ~MojoWatcher() override;

  MojoResult cancel();

  void Trace(blink::Visitor*) override;

  // ActiveScriptWrappable
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) final;

 private:
  friend class V8MojoWatcher;

  MojoResult Watch(mojo::Handle, const MojoHandleSignals*);
  MojoResult Arm(MojoResult* ready_result);

  static void OnHandleReady(const MojoTrapEvent*);
  void RunReadyCallback(MojoResult);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  Member<V8MojoWatchCallback> callback_;
  mojo::ScopedTrapHandle trap_handle_;
  mojo::Handle handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MOJO_MOJO_WATCHER_H_
