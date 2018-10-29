// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator.h"
#include "third_party/blink/renderer/platform/graphics/compositor_mutator_client.h"
#include "third_party/blink/renderer/platform/graphics/main_thread_mutator_client.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/waitable_event.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

AnimationWorkletMutatorDispatcherImpl::AnimationWorkletMutatorDispatcherImpl(
    bool main_thread_task_runner)
    : client_(nullptr), weak_factory_(this) {
  // By default layout tests run without threaded compositing. See
  // https://crbug.com/770028 For these situations we run on the Main thread.
  host_queue_ =
      main_thread_task_runner || !Platform::Current()->CompositorThread()
          ? Platform::Current()->MainThread()->GetTaskRunner()
          : Platform::Current()->CompositorThread()->GetTaskRunner();
}

AnimationWorkletMutatorDispatcherImpl::
    ~AnimationWorkletMutatorDispatcherImpl() {}

// static
template <typename ClientType>
std::unique_ptr<ClientType> AnimationWorkletMutatorDispatcherImpl::CreateClient(
    base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>* weak_interface,
    scoped_refptr<base::SingleThreadTaskRunner>* queue,
    bool main_thread_client) {
  DCHECK(IsMainThread());
  auto mutator = std::make_unique<AnimationWorkletMutatorDispatcherImpl>(
      main_thread_client);
  // This is allowed since we own the class for the duration of creation.
  *weak_interface = mutator->weak_factory_.GetWeakPtr();
  *queue = mutator->GetTaskRunner();

  return std::make_unique<ClientType>(std::move(mutator));
}

// static
std::unique_ptr<CompositorMutatorClient>
AnimationWorkletMutatorDispatcherImpl::CreateCompositorThreadClient(
    base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>* weak_interface,
    scoped_refptr<base::SingleThreadTaskRunner>* queue) {
  return CreateClient<CompositorMutatorClient>(weak_interface, queue, false);
}

// static
std::unique_ptr<MainThreadMutatorClient>
AnimationWorkletMutatorDispatcherImpl::CreateMainThreadClient(
    base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>* weak_interface,
    scoped_refptr<base::SingleThreadTaskRunner>* queue) {
  return CreateClient<MainThreadMutatorClient>(weak_interface, queue, true);
}

void AnimationWorkletMutatorDispatcherImpl::Mutate(
    std::unique_ptr<AnimationWorkletDispatcherInput> mutator_input) {
  TRACE_EVENT0("cc", "AnimationWorkletMutatorDispatcherImpl::mutate");
  if (mutator_map_.IsEmpty())
    return;
  base::ElapsedTimer timer;
  DCHECK(client_);

  Vector<std::unique_ptr<AnimationWorkletDispatcherOutput>> outputs(
      mutator_map_.size());
  Vector<WaitableEvent> done_events(mutator_map_.size());

  int index = 0;
  for (auto& pair : mutator_map_) {
    AnimationWorkletMutator* mutator = pair.key;
    scoped_refptr<base::SingleThreadTaskRunner> worklet_queue = pair.value;

    std::unique_ptr<AnimationWorkletInput> input =
        mutator_input->TakeWorkletState(mutator->GetScopeId());

    DCHECK(!worklet_queue->BelongsToCurrentThread());
    std::unique_ptr<AutoSignal> done =
        std::make_unique<AutoSignal>(&done_events[index]);
    std::unique_ptr<AnimationWorkletDispatcherOutput>& output = outputs[index];

    if (input) {
      PostCrossThreadTask(
          *worklet_queue, FROM_HERE,
          CrossThreadBind(
              [](AnimationWorkletMutator* mutator,
                 std::unique_ptr<AnimationWorkletInput> input,
                 std::unique_ptr<AutoSignal> completion,
                 std::unique_ptr<AnimationWorkletDispatcherOutput>* output) {
                *output = mutator->Mutate(std::move(input));
              },
              WrapCrossThreadWeakPersistent(mutator),
              WTF::Passed(std::move(input)), WTF::Passed(std::move(done)),
              CrossThreadUnretained(&output)));
    }
    index++;
  }

  for (WaitableEvent& event : done_events) {
    event.Wait();
  }

  for (auto& output : outputs) {
    // Animator that has no input does not produce any output.
    if (!output)
      continue;
    client_->SetMutationUpdate(std::move(output));
  }

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Animation.AnimationWorklet.Dispatcher.SynchronousMutateDuration",
      timer.Elapsed(), base::TimeDelta::FromMicroseconds(1),
      base::TimeDelta::FromMilliseconds(100), 50);
}

void AnimationWorkletMutatorDispatcherImpl::RegisterAnimationWorkletMutator(
    CrossThreadPersistent<AnimationWorkletMutator> mutator,
    scoped_refptr<base::SingleThreadTaskRunner> mutator_runner) {
  TRACE_EVENT0(
      "cc",
      "AnimationWorkletMutatorDispatcherImpl::RegisterAnimationWorkletMutator");

  DCHECK(mutator);
  DCHECK(host_queue_->BelongsToCurrentThread());

  mutator_map_.insert(mutator, mutator_runner);
}

void AnimationWorkletMutatorDispatcherImpl::UnregisterAnimationWorkletMutator(
    CrossThreadPersistent<AnimationWorkletMutator> mutator) {
  TRACE_EVENT0("cc",
               "AnimationWorkletMutatorDispatcherImpl::"
               "UnregisterAnimationWorkletMutator");
  DCHECK(mutator);
  DCHECK(host_queue_->BelongsToCurrentThread());

  mutator_map_.erase(mutator);
}

bool AnimationWorkletMutatorDispatcherImpl::HasMutators() {
  return !mutator_map_.IsEmpty();
}

AnimationWorkletMutatorDispatcherImpl::AutoSignal::AutoSignal(
    WaitableEvent* event)
    : event_(event) {
  DCHECK(event);
}

AnimationWorkletMutatorDispatcherImpl::AutoSignal::~AutoSignal() {
  event_->Signal();
}

}  // namespace blink
