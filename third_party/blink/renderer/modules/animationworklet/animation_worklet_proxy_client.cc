// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_proxy_client.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

static const wtf_size_t kMaxMutateCountToSwitch = 10u;

}  // end namespace

/* static */
const char AnimationWorkletProxyClient::kSupplementName[] =
    "AnimationWorkletProxyClient";

/* static */
const int8_t AnimationWorkletProxyClient::kNumStatelessGlobalScopes = 2;

AnimationWorkletProxyClient::AnimationWorkletProxyClient(
    int worklet_id,
    base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
        compositor_mutator_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_mutator_runner,
    base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
        main_thread_mutator_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_mutator_runner)
    : Supplement(nullptr),
      worklet_id_(worklet_id),
      state_(RunState::kUninitialized),
      next_global_scope_switch_countdown_(0),
      current_global_scope_index_(0) {
  DCHECK(IsMainThread());

  // The dispatchers are weak pointers that may come from another thread. It's
  // illegal to check them here. Instead, the task runners are checked.
  if (compositor_mutator_runner) {
    mutator_items_.emplace_back(std::move(compositor_mutator_dispatcher),
                                std::move(compositor_mutator_runner));
  }
  if (main_thread_mutator_runner) {
    mutator_items_.emplace_back(std::move(main_thread_mutator_dispatcher),
                                std::move(main_thread_mutator_runner));
  }
}

void AnimationWorkletProxyClient::Trace(Visitor* visitor) const {
  Supplement<WorkerClients>::Trace(visitor);
  AnimationWorkletMutator::Trace(visitor);
}

void AnimationWorkletProxyClient::SynchronizeAnimatorName(
    const String& animator_name) {
  if (state_ == RunState::kDisposed)
    return;
  // Only proceed to synchronization when the animator has been registered on
  // all global scopes.
  auto* it = registered_animators_.insert(animator_name, 0).stored_value;
  ++it->value;
  if (it->value != kNumStatelessGlobalScopes) {
    DCHECK_LT(it->value, kNumStatelessGlobalScopes)
        << "We should not have registered the same name more than the number "
           "of scopes times.";
    return;
  }

  // Animator registration is processed before the loading promise being
  // resolved which is also done with a posted task (See
  // WorkletModuleTreeClient::NotifyModuleTreeLoadFinished). Since both are
  // posted task and a SequencedTaskRunner is used, we are guaranteed that
  // registered names are synced before resolving the load promise therefore it
  // is safe to use a post task here.
  for (auto& mutator_item : mutator_items_) {
    PostCrossThreadTask(
        *mutator_item.mutator_runner, FROM_HERE,
        CrossThreadBindOnce(
            &AnimationWorkletMutatorDispatcherImpl::SynchronizeAnimatorName,
            mutator_item.mutator_dispatcher, animator_name));
  }
}

void AnimationWorkletProxyClient::AddGlobalScope(
    WorkletGlobalScope* global_scope) {
  DCHECK(global_scope);
  DCHECK(global_scope->IsContextThread());
  if (state_ == RunState::kDisposed)
    return;

  global_scopes_.push_back(To<AnimationWorkletGlobalScope>(global_scope));

  if (state_ != RunState::kUninitialized) {
    return;
  }

  // Wait for all global scopes to load before proceeding with registration.
  if (global_scopes_.size() < kNumStatelessGlobalScopes) {
    return;
  }

  // TODO(majidvp): Add an AnimationWorklet task type when the spec is final.
  scoped_refptr<base::SingleThreadTaskRunner> global_scope_runner =
      global_scope->GetThread()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  state_ = RunState::kWorking;

  for (auto& mutator_item : mutator_items_) {
    PostCrossThreadTask(
        *mutator_item.mutator_runner, FROM_HERE,
        CrossThreadBindOnce(&AnimationWorkletMutatorDispatcherImpl::
                                RegisterAnimationWorkletMutator,
                            mutator_item.mutator_dispatcher,
                            WrapCrossThreadPersistent(this),
                            global_scope_runner));
  }
}

void AnimationWorkletProxyClient::Dispose() {
  if (state_ == RunState::kWorking) {
    // At worklet scope termination break the reference to the clients if it is
    // still alive.
    for (auto& mutator_item : mutator_items_) {
      PostCrossThreadTask(
          *mutator_item.mutator_runner, FROM_HERE,
          CrossThreadBindOnce(&AnimationWorkletMutatorDispatcherImpl::
                                  UnregisterAnimationWorkletMutator,
                              mutator_item.mutator_dispatcher,
                              WrapCrossThreadPersistent(this)));
    }
  }
  state_ = RunState::kDisposed;

  // At worklet scope termination break the reference cycle between
  // AnimationWorkletGlobalScope and AnimationWorkletProxyClient.
  global_scopes_.clear();
  mutator_items_.clear();
  registered_animators_.clear();
}

std::unique_ptr<AnimationWorkletOutput> AnimationWorkletProxyClient::Mutate(
    std::unique_ptr<AnimationWorkletInput> input) {
  std::unique_ptr<AnimationWorkletOutput> output =
      std::make_unique<AnimationWorkletOutput>();

  if (state_ == RunState::kDisposed)
    return output;

  DCHECK(input);
#if DCHECK_IS_ON()
  DCHECK(input->ValidateId(worklet_id_))
      << "Input has state that does not belong to this global scope: "
      << worklet_id_;
#endif

  AnimationWorkletGlobalScope* global_scope =
      SelectGlobalScopeAndUpdateAnimatorsIfNecessary();
  DCHECK(global_scope);
  // Create or destroy instances of animators on current global scope.
  global_scope->UpdateAnimatorsList(*input);

  global_scope->UpdateAnimators(*input, output.get(),
                                [](Animator* animator) { return true; });
  return output;
}

AnimationWorkletGlobalScope*
AnimationWorkletProxyClient::SelectGlobalScopeAndUpdateAnimatorsIfNecessary() {
  if (--next_global_scope_switch_countdown_ < 0) {
    int last_global_scope_index = current_global_scope_index_;
    current_global_scope_index_ =
        (current_global_scope_index_ + 1) % global_scopes_.size();
    global_scopes_[last_global_scope_index]->MigrateAnimatorsTo(
        global_scopes_[current_global_scope_index_]);
    // Introduce an element of randomness in the switching interval to make
    // stateful dependences easier to spot.
    next_global_scope_switch_countdown_ =
        base::RandInt(0, kMaxMutateCountToSwitch - 1);
  }
  return global_scopes_[current_global_scope_index_];
}

void AnimationWorkletProxyClient::AddGlobalScopeForTesting(
    WorkletGlobalScope* global_scope) {
  DCHECK(global_scope);
  DCHECK(global_scope->IsContextThread());
  global_scopes_.push_back(To<AnimationWorkletGlobalScope>(global_scope));
}

// static
AnimationWorkletProxyClient* AnimationWorkletProxyClient::FromDocument(
    Document* document,
    int worklet_id) {
  WebLocalFrameImpl* local_frame =
      WebLocalFrameImpl::FromFrame(document->GetFrame());

  // By default web tests run without threaded compositing. See
  // https://crbug.com/770028. If threaded compositing is disabled, we
  // run on the main thread's compositor task runner otherwise we run
  // tasks on the compositor thread's default task runner.
  scoped_refptr<base::SingleThreadTaskRunner> compositor_host_queue =
      Thread::CompositorThread()
          ? Thread::CompositorThread()->GetTaskRunner()
          : local_frame->GetAgentGroupScheduler()->CompositorTaskRunner();
  base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
      compositor_mutator_dispatcher =
          local_frame->LocalRootFrameWidget()
              ->EnsureCompositorMutatorDispatcher(compositor_host_queue);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_host_queue =
      local_frame->GetAgentGroupScheduler()->CompositorTaskRunner();
  base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
      main_thread_mutator_dispatcher =
          document->GetWorkletAnimationController()
              .EnsureMainThreadMutatorDispatcher(main_thread_host_queue);

  return MakeGarbageCollected<AnimationWorkletProxyClient>(
      worklet_id, std::move(compositor_mutator_dispatcher),
      std::move(compositor_host_queue),
      std::move(main_thread_mutator_dispatcher),
      std::move(main_thread_host_queue));
}

AnimationWorkletProxyClient* AnimationWorkletProxyClient::From(
    WorkerClients* clients) {
  return Supplement<WorkerClients>::From<AnimationWorkletProxyClient>(clients);
}

void ProvideAnimationWorkletProxyClientTo(WorkerClients* clients,
                                          AnimationWorkletProxyClient* client) {
  clients->ProvideSupplement(client);
}

}  // namespace blink
