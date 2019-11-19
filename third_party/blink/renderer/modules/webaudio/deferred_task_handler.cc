/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/deferred_task_handler.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

void DeferredTaskHandler::lock() {
  // Don't allow regular lock in real-time audio thread.
  DCHECK(!IsAudioThread());
  context_graph_mutex_.lock();
}

bool DeferredTaskHandler::TryLock() {
  // Try to catch cases of using try lock on main thread
  // - it should use regular lock.
  DCHECK(IsAudioThread());
  if (!IsAudioThread()) {
    // In release build treat tryLock() as lock() (since above
    // DCHECK(isAudioThread) never fires) - this is the best we can do.
    lock();
    return true;
  }
  return context_graph_mutex_.TryLock();
}

void DeferredTaskHandler::unlock() {
  context_graph_mutex_.unlock();
}

void DeferredTaskHandler::OfflineLock() {
  // CHECK is here to make sure to explicitly crash if this is called from
  // other than the offline render thread, which is considered as the audio
  // thread in OfflineAudioContext.
  CHECK(IsAudioThread()) << "DeferredTaskHandler::offlineLock() must be called "
                            "within the offline audio thread.";

  context_graph_mutex_.lock();
}

void DeferredTaskHandler::BreakConnections() {
  DCHECK(IsAudioThread());
  AssertGraphOwner();

  // Remove any finished handlers from the active handlers list and break the
  // connection.
  wtf_size_t size = finished_source_handlers_.size();
  if (size > 0) {
    for (auto* finished : finished_source_handlers_) {
      active_source_handlers_.erase(finished);
      finished->BreakConnectionWithLock();
    }
    finished_source_handlers_.clear();
  }
}

void DeferredTaskHandler::MarkSummingJunctionDirty(
    AudioSummingJunction* summing_junction) {
  AssertGraphOwner();
  dirty_summing_junctions_.insert(summing_junction);
}

void DeferredTaskHandler::RemoveMarkedSummingJunction(
    AudioSummingJunction* summing_junction) {
  DCHECK(IsMainThread());
  AssertGraphOwner();
  dirty_summing_junctions_.erase(summing_junction);
}

void DeferredTaskHandler::MarkAudioNodeOutputDirty(AudioNodeOutput* output) {
  DCHECK(IsMainThread());
  AssertGraphOwner();
  dirty_audio_node_outputs_.insert(output);
}

void DeferredTaskHandler::RemoveMarkedAudioNodeOutput(AudioNodeOutput* output) {
  DCHECK(IsMainThread());
  AssertGraphOwner();
  dirty_audio_node_outputs_.erase(output);
}

void DeferredTaskHandler::HandleDirtyAudioSummingJunctions() {
  AssertGraphOwner();
  for (AudioSummingJunction* junction : dirty_summing_junctions_)
    junction->UpdateRenderingState();
  dirty_summing_junctions_.clear();
}

void DeferredTaskHandler::HandleDirtyAudioNodeOutputs() {
  AssertGraphOwner();

  HashSet<AudioNodeOutput*> dirty_outputs;
  dirty_audio_node_outputs_.swap(dirty_outputs);

  // Note: the updating of rendering state may cause output nodes
  // further down the chain to be marked as dirty. These will not
  // be processed in this render quantum.
  for (AudioNodeOutput* output : dirty_outputs)
    output->UpdateRenderingState();
}

void DeferredTaskHandler::AddAutomaticPullNode(
    scoped_refptr<AudioHandler> node) {
  AssertGraphOwner();

  if (!automatic_pull_handlers_.Contains(node)) {
    automatic_pull_handlers_.insert(node);
    automatic_pull_handlers_need_updating_ = true;
  }
}

void DeferredTaskHandler::RemoveAutomaticPullNode(AudioHandler* node) {
  AssertGraphOwner();

  auto it = automatic_pull_handlers_.find(node);
  if (it != automatic_pull_handlers_.end()) {
    automatic_pull_handlers_.erase(it);
    automatic_pull_handlers_need_updating_ = true;
  }
}

void DeferredTaskHandler::UpdateAutomaticPullNodes() {
  AssertGraphOwner();

  if (automatic_pull_handlers_need_updating_) {
    CopyToVector(automatic_pull_handlers_, rendering_automatic_pull_handlers_);
    automatic_pull_handlers_need_updating_ = false;
  }
}

void DeferredTaskHandler::ProcessAutomaticPullNodes(
    uint32_t frames_to_process) {
  DCHECK(IsAudioThread());

  for (unsigned i = 0; i < rendering_automatic_pull_handlers_.size(); ++i) {
    rendering_automatic_pull_handlers_[i]->ProcessIfNecessary(
        frames_to_process);
  }
}

void DeferredTaskHandler::AddTailProcessingHandler(
    scoped_refptr<AudioHandler> handler) {
  DCHECK(accepts_tail_processing_);
  AssertGraphOwner();

  if (!tail_processing_handlers_.Contains(handler)) {
#if DEBUG_AUDIONODE_REFERENCES > 1
    handler->AddTailProcessingDebug();
#endif
    tail_processing_handlers_.push_back(handler);
  }
}

void DeferredTaskHandler::RemoveTailProcessingHandler(AudioHandler* handler,
                                                      bool disable_outputs) {
  AssertGraphOwner();

  wtf_size_t index = tail_processing_handlers_.Find(handler);
  if (index != kNotFound) {
#if DEBUG_AUDIONODE_REFERENCES > 1
    handler->RemoveTailProcessingDebug(disable_outputs);
#endif

    if (disable_outputs) {
      // Disabling of outputs should happen on the main thread so save this
      // handler so it can be processed there.
      finished_tail_processing_handlers_.push_back(
          std::move(tail_processing_handlers_[index]));
    }
    tail_processing_handlers_.EraseAt(index);

    return;
  }

  // Check finished tail handlers and remove this handler from the list so that
  // we don't disable outputs later when these are processed.
  index = finished_tail_processing_handlers_.Find(handler);
  if (index != kNotFound) {
#if DEBUG_AUDIONODE_REFERENCES > 1
    handler->RemoveTailProcessingDebug(disable_outputs);
#endif
    finished_tail_processing_handlers_.EraseAt(index);
    return;
  }
}

void DeferredTaskHandler::UpdateTailProcessingHandlers() {
  DCHECK(IsAudioThread());

  for (unsigned k = tail_processing_handlers_.size(); k > 0; --k) {
    scoped_refptr<AudioHandler> handler = tail_processing_handlers_[k - 1];
    if (handler->PropagatesSilence()) {
#if DEBUG_AUDIONODE_REFERENCES
      fprintf(stderr,
              "[%16p]: %16p: %2d: updateTail @%.15g (tail = %.15g + %.15g)\n",
              handler->Context(), handler.get(), handler->GetNodeType(),
              handler->Context()->currentTime(), handler->TailTime(),
              handler->LatencyTime());
#endif
      RemoveTailProcessingHandler(handler.get(), true);
    }
  }
}

void DeferredTaskHandler::AddChangedChannelCountMode(AudioHandler* node) {
  DCHECK(IsMainThread());
  AssertGraphOwner();
  deferred_count_mode_change_.insert(node);
}

void DeferredTaskHandler::RemoveChangedChannelCountMode(AudioHandler* node) {
  AssertGraphOwner();
  deferred_count_mode_change_.erase(node);
}

void DeferredTaskHandler::AddChangedChannelInterpretation(AudioHandler* node) {
  DCHECK(IsMainThread());
  AssertGraphOwner();
  deferred_channel_interpretation_change_.insert(node);
}

void DeferredTaskHandler::RemoveChangedChannelInterpretation(
    AudioHandler* node) {
  AssertGraphOwner();
  deferred_channel_interpretation_change_.erase(node);
}

void DeferredTaskHandler::UpdateChangedChannelCountMode() {
  AssertGraphOwner();
  for (AudioHandler* node : deferred_count_mode_change_)
    node->UpdateChannelCountMode();
  deferred_count_mode_change_.clear();
}

void DeferredTaskHandler::UpdateChangedChannelInterpretation() {
  AssertGraphOwner();
  for (AudioHandler* node : deferred_channel_interpretation_change_)
    node->UpdateChannelInterpretation();
  deferred_channel_interpretation_change_.clear();
}

DeferredTaskHandler::DeferredTaskHandler(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : automatic_pull_handlers_need_updating_(false),
      task_runner_(std::move(task_runner)),
      audio_thread_(0) {}

scoped_refptr<DeferredTaskHandler> DeferredTaskHandler::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return base::AdoptRef(new DeferredTaskHandler(std::move(task_runner)));
}

DeferredTaskHandler::~DeferredTaskHandler() = default;

void DeferredTaskHandler::HandleDeferredTasks() {
  UpdateChangedChannelCountMode();
  UpdateChangedChannelInterpretation();
  HandleDirtyAudioSummingJunctions();
  HandleDirtyAudioNodeOutputs();
  UpdateAutomaticPullNodes();
  UpdateTailProcessingHandlers();
}

void DeferredTaskHandler::ContextWillBeDestroyed() {
  for (auto& handler : rendering_orphan_handlers_)
    handler->ClearContext();
  for (auto& handler : deletable_orphan_handlers_)
    handler->ClearContext();
  ClearHandlersToBeDeleted();
  // Some handlers might live because of their cross thread tasks.
}

DeferredTaskHandler::GraphAutoLocker::GraphAutoLocker(
    const BaseAudioContext* context)
    : handler_(context->GetDeferredTaskHandler()) {
  handler_.lock();
}

DeferredTaskHandler::OfflineGraphAutoLocker::OfflineGraphAutoLocker(
    OfflineAudioContext* context)
    : handler_(context->GetDeferredTaskHandler()) {
  handler_.OfflineLock();
}

void DeferredTaskHandler::AddRenderingOrphanHandler(
    scoped_refptr<AudioHandler> handler) {
  DCHECK(handler);
  DCHECK(!rendering_orphan_handlers_.Contains(handler));
  rendering_orphan_handlers_.push_back(std::move(handler));
}

void DeferredTaskHandler::RequestToDeleteHandlersOnMainThread() {
  DCHECK(IsAudioThread());
  AssertGraphOwner();

  // Quick exit if there are no handlers that need to be deleted so that we
  // don't unecessarily post a task.  Be consistent with
  // |DeleteHandlersOnMainThread()| so we don't accidentally return early when
  // there are handlers that could be deleted.
  if (rendering_orphan_handlers_.IsEmpty() &&
      finished_tail_processing_handlers_.size() == 0) {
    return;
  }

  deletable_orphan_handlers_.AppendVector(rendering_orphan_handlers_);
  rendering_orphan_handlers_.clear();
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(&DeferredTaskHandler::DeleteHandlersOnMainThread,
                          scoped_refptr<DeferredTaskHandler>(this)));
}

void DeferredTaskHandler::DeleteHandlersOnMainThread() {
  DCHECK(IsMainThread());
  GraphAutoLocker locker(*this);
  deletable_orphan_handlers_.clear();
  DisableOutputsForTailProcessing();
}

void DeferredTaskHandler::ClearHandlersToBeDeleted() {
  DCHECK(IsMainThread());
  GraphAutoLocker locker(*this);
  tail_processing_handlers_.clear();
  rendering_orphan_handlers_.clear();
  deletable_orphan_handlers_.clear();
  automatic_pull_handlers_.clear();
  rendering_automatic_pull_handlers_.clear();
  active_source_handlers_.clear();
}

void DeferredTaskHandler::SetAudioThreadToCurrentThread() {
  DCHECK(!IsMainThread());
  audio_thread_.store(CurrentThread(), std::memory_order_relaxed);
}

void DeferredTaskHandler::DisableOutputsForTailProcessing() {
  DCHECK(IsMainThread());
  // Tail processing nodes have finished processing their tails so we need to
  // disable their outputs to indicate to downstream nodes that they're done.
  // This has to be done in the main thread because DisableOutputs() can cause
  // summing juctions to go away, which must be done on the main thread.
  for (auto handler : finished_tail_processing_handlers_) {
#if DEBUG_AUDIONODE_REFERENCES > 1
    fprintf(stderr, "[%16p]: %16p: %2d: DisableOutputsForTailProcessing @%g\n",
            handler->Context(), handler.get(), handler->GetNodeType(),
            handler->Context()->currentTime());
#endif
    handler->DisableOutputs();
  }
  finished_tail_processing_handlers_.clear();
}

void DeferredTaskHandler::FinishTailProcessing() {
  DCHECK(IsMainThread());
  // DisableOutputs must run with the graph lock.
  GraphAutoLocker locker(*this);

  // TODO(crbug.com/832200): Simplify this!

  // |DisableOutputs()| can cause new handlers to start tail processing, which
  // in turn can cause hte handler to want to disable outputs.  For the former
  // case, the handler is added to |tail_processing_handlers_|.  In the latter
  // case, the handler is added to |finished_tail_processing_handlers_|.  So, we
  // need to loop around until these vectors are completely empty.
  do {
    while (tail_processing_handlers_.size() > 0) {
      // |DisableOutputs()| can modify |tail_processing_handlers_|, so
      // swap it out before processing it.  And keep running this until
      // nothing gets added to |tail_processing_handlers_|.
      Vector<scoped_refptr<AudioHandler>> handlers_to_be_disabled;

      handlers_to_be_disabled.swap(tail_processing_handlers_);
      for (auto& handler : handlers_to_be_disabled)
        handler->DisableOutputs();
    }
    DisableOutputsForTailProcessing();
  } while (tail_processing_handlers_.size() > 0 ||
           finished_tail_processing_handlers_.size() > 0);
}

}  // namespace blink
