// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_ANIMATION_WORKLET_MUTATOR_DISPATCHER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_ANIMATION_WORKLET_MUTATOR_DISPATCHER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/mutator_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class CompositorMutatorClient;
class MainThreadMutatorClient;
class WaitableEvent;

// Fans out requests to all of the registered AnimationWorkletMutators which can
// then run worklet animations to produce mutation updates. Requests for
// animation frames are received from AnimationWorkletMutators and generate a
// new frame.
class PLATFORM_EXPORT AnimationWorkletMutatorDispatcherImpl final
    : public AnimationWorkletMutatorDispatcher {
 public:
  // There are three outputs for the two interface surfaces of the created
  // class blob. The returned owning pointer to the Client, which
  // also owns the rest of the structure. |mutatee| and |mutatee_runner| form a
  // pair for referencing the AnimationWorkletMutatorDispatcherImpl. i.e. Put
  // tasks on the TaskRunner using the WeakPtr to get to the methods.
  static std::unique_ptr<CompositorMutatorClient> CreateCompositorThreadClient(
      base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>* mutatee,
      scoped_refptr<base::SingleThreadTaskRunner>* mutatee_runner);
  static std::unique_ptr<MainThreadMutatorClient> CreateMainThreadClient(
      base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>* mutatee,
      scoped_refptr<base::SingleThreadTaskRunner>* mutatee_runner);

  explicit AnimationWorkletMutatorDispatcherImpl(bool main_thread_task_runner);
  ~AnimationWorkletMutatorDispatcherImpl() override;

  // AnimationWorkletMutatorDispatcher implementation.
  void Mutate(std::unique_ptr<AnimationWorkletDispatcherInput>) override;
  // TODO(majidvp): Remove when timeline inputs are known.
  bool HasMutators() override;

  // Interface for use by the AnimationWorklet Thread(s) to request calls.
  // (To the given Mutator on the given TaskRunner.)
  void RegisterAnimationWorkletMutator(
      CrossThreadPersistent<AnimationWorkletMutator>,
      scoped_refptr<base::SingleThreadTaskRunner> mutator_runner);

  void UnregisterAnimationWorkletMutator(
      CrossThreadPersistent<AnimationWorkletMutator>);

  void SetClient(MutatorClient* client) { client_ = client; }

 private:
  using AnimationWorkletMutatorToTaskRunnerMap =
      HashMap<CrossThreadPersistent<AnimationWorkletMutator>,
              scoped_refptr<base::SingleThreadTaskRunner>>;

  class AutoSignal {
   public:
    explicit AutoSignal(WaitableEvent*);
    ~AutoSignal();

   private:
    WaitableEvent* event_;

    DISALLOW_COPY_AND_ASSIGN(AutoSignal);
  };

  // The AnimationWorkletProxyClients are also owned by the WorkerClients
  // dictionary.
  AnimationWorkletMutatorToTaskRunnerMap mutator_map_;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return host_queue_;
  }

  template <typename ClientType>
  static std::unique_ptr<ClientType> CreateClient(
      base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>* weak_interface,
      scoped_refptr<base::SingleThreadTaskRunner>* queue,
      bool create_main_thread_client);

  scoped_refptr<base::SingleThreadTaskRunner> host_queue_;

  // The MutatorClient owns (std::unique_ptr) us, so this pointer is
  // valid as long as this class exists.
  MutatorClient* client_;

  base::WeakPtrFactory<AnimationWorkletMutatorDispatcherImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AnimationWorkletMutatorDispatcherImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_ANIMATION_WORKLET_MUTATOR_DISPATCHER_IMPL_H_
