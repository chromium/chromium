// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_ANIMATION_WORKLET_PROXY_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_ANIMATION_WORKLET_PROXY_CLIENT_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_global_scope.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator.h"

namespace blink {

class AnimationWorkletMutatorDispatcherImpl;
class Document;
class WorkletGlobalScope;

// Mediates between animation worklet global scope and its associated
// dispatchers. An AnimationWorkletProxyClient is associated with a single
// global scope and up to two dispatchers representing main and compositor
// threads.
//
// This is constructed on the main thread but it is used in the worklet backing
// thread.
class MODULES_EXPORT AnimationWorkletProxyClient
    : public GarbageCollectedFinalized<AnimationWorkletProxyClient>,
      public Supplement<WorkerClients>,
      public AnimationWorkletMutator {
  USING_GARBAGE_COLLECTED_MIXIN(AnimationWorkletProxyClient);
  DISALLOW_COPY_AND_ASSIGN(AnimationWorkletProxyClient);

 public:
  static const char kSupplementName[];

  // This client is hooked to the given |mutatee|, on the given
  // |mutatee_runner|.
  explicit AnimationWorkletProxyClient(
      int scope_id,
      base::WeakPtr<AnimationWorkletMutatorDispatcherImpl> compositor_mutatee,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_mutatee_runner,
      base::WeakPtr<AnimationWorkletMutatorDispatcherImpl> main_thread_mutatee,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_mutatee_runner);
  void Trace(blink::Visitor*) override;

  virtual void SetGlobalScope(WorkletGlobalScope*);
  void Dispose();

  // AnimationWorkletMutator:
  // These methods are invoked on the animation worklet thread.
  int GetScopeId() const override { return scope_id_; }
  std::unique_ptr<AnimationWorkletOutput> Mutate(
      std::unique_ptr<AnimationWorkletInput> input) override;

  static AnimationWorkletProxyClient* FromDocument(Document*, int scope_id);
  static AnimationWorkletProxyClient* From(WorkerClients*);

 private:
  const int scope_id_;

  struct MutatorItem {
    base::WeakPtr<AnimationWorkletMutatorDispatcherImpl> mutator_dispatcher;
    scoped_refptr<base::SingleThreadTaskRunner> mutator_runner;
    MutatorItem(
        base::WeakPtr<AnimationWorkletMutatorDispatcherImpl> mutator_dispatcher,
        scoped_refptr<base::SingleThreadTaskRunner> mutator_runner)
        : mutator_dispatcher(std::move(mutator_dispatcher)),
          mutator_runner(std::move(mutator_runner)) {}
  };
  WTF::Vector<MutatorItem> mutator_items_;

  CrossThreadPersistent<AnimationWorkletGlobalScope> global_scope_;

  enum RunState { kUninitialized, kWorking, kDisposed } state_;
};

void MODULES_EXPORT
ProvideAnimationWorkletProxyClientTo(WorkerClients*,
                                     AnimationWorkletProxyClient*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_ANIMATION_WORKLET_PROXY_CLIENT_H_
