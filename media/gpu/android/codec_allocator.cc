// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_allocator.h"

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/limits.h"
#include "media/base/media.h"
#include "media/base/timestamp_constants.h"
#include "media/gpu/android/android_video_decode_accelerator.h"

namespace media {

namespace {

// Give tasks 800ms before considering them hung. MediaCodec.configure() calls
// typically take 100-200ms on a N5, so 800ms is expected to very rarely result
// in false positives. Also, false positives have low impact because we resume
// using the thread when the task completes.
constexpr base::TimeDelta kHungTaskDetectionTimeout =
    base::TimeDelta::FromMilliseconds(800);

// This must be safe to call on any thread. Returns nullptr on failure.
std::unique_ptr<MediaCodecBridge> CreateMediaCodecInternal(
    CodecAllocator::CodecFactoryCB factory_cb,
    scoped_refptr<CodecConfig> codec_config,
    bool requires_software_codec) {
  TRACE_EVENT0("media", "CodecAllocator::CreateMediaCodec");

  const base::android::JavaRef<jobject>& media_crypto =
      codec_config->media_crypto ? *codec_config->media_crypto : nullptr;

  // |requires_secure_codec| implies that it's an encrypted stream.
  DCHECK(!codec_config->requires_secure_codec || !media_crypto.is_null());

  CodecType codec_type = CodecType::kAny;
  if (codec_config->requires_secure_codec && requires_software_codec) {
    DVLOG(1) << "Secure software codec doesn't exist.";
    return nullptr;
  } else if (codec_config->requires_secure_codec) {
    codec_type = CodecType::kSecure;
  } else if (requires_software_codec) {
    codec_type = CodecType::kSoftware;
  }

  std::unique_ptr<MediaCodecBridge> codec(factory_cb.Run(
      codec_config->codec, codec_type,
      codec_config->initial_expected_coded_size,
      codec_config->surface_bundle->GetJavaSurface(), media_crypto,
      codec_config->csd0, codec_config->csd1,
      codec_config->container_color_space, codec_config->hdr_metadata, true,
      codec_config->on_buffers_available_cb));

  return codec;
}

// Delete |codec| and signal |done_event| if it's not null.
void DeleteMediaCodecAndSignal(std::unique_ptr<MediaCodecBridge> codec,
                               base::WaitableEvent* done_event) {
  TRACE_EVENT0("media", "CodecAllocator::DeleteMediaCodec");
  codec.reset();
  if (done_event)
    done_event->Signal();
}

}  // namespace

CodecConfig::CodecConfig() {}
CodecConfig::~CodecConfig() {}

CodecAllocator::HangDetector::HangDetector(const base::TickClock* tick_clock)
    : tick_clock_(tick_clock) {}

void CodecAllocator::HangDetector::WillProcessTask(
    const base::PendingTask& pending_task) {
  base::AutoLock l(lock_);
  task_start_time_ = tick_clock_->NowTicks();
}

void CodecAllocator::HangDetector::DidProcessTask(
    const base::PendingTask& pending_task) {
  base::AutoLock l(lock_);
  task_start_time_ = base::TimeTicks();
}

bool CodecAllocator::HangDetector::IsThreadLikelyHung() {
  base::AutoLock l(lock_);
  if (task_start_time_.is_null())
    return false;

  return (tick_clock_->NowTicks() - task_start_time_) >
         kHungTaskDetectionTimeout;
}

// static
CodecAllocator* CodecAllocator::GetInstance(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  static CodecAllocator* allocator = new CodecAllocator(
      base::BindRepeating(&MediaCodecBridgeImpl::CreateVideoDecoder),
      task_runner);

  // Verify that this caller agrees on the task runner, if one was specified.
  DCHECK(!task_runner || allocator->task_runner_ == task_runner);

  return allocator;
}

void CodecAllocator::StartThread(CodecAllocatorClient* client) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&CodecAllocator::StartThread,
                                          base::Unretained(this), client));
    return;
  }

  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // NOTE: |client| might not be a valid pointer anymore.  All we know is that
  // no other client is aliased to it, as long as |client| called StopThread
  // before it was destroyed.  The reason is that any re-use of |client| would
  // have to also post StartThread to this thread.  Since the re-use must be
  // ordered later with respect to deleting the original |client|, the post must
  // also be ordered later.  So, there might be an aliased client posted, but it
  // won't have started yet.

  // Cancel any pending StopThreadTask()s because we need the threads now.
  weak_this_factory_.InvalidateWeakPtrs();

  // Try to start the threads if they haven't been started.
  for (auto* thread : threads_) {
    if (thread->thread.IsRunning())
      continue;

    if (!thread->thread.Start())
      return;

    // Register the hang detector to observe the thread's MessageLoop.
    thread->thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&base::MessageLoop::AddTaskObserver,
                       base::Unretained(thread->thread.message_loop()),
                       &thread->hang_detector));
  }

  clients_.insert(client);
  return;
}

void CodecAllocator::StopThread(CodecAllocatorClient* client) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&CodecAllocator::StopThread,
                                          base::Unretained(this), client));
    return;
  }

  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  clients_.erase(client);
  if (!clients_.empty()) {
    // If we aren't stopping, then signal immediately.
    if (stop_event_for_testing_)
      stop_event_for_testing_->Signal();
    return;
  }

  // Post a task to stop each thread through its task runner and back to this
  // thread. This ensures that all pending tasks are run first. If a new AVDA
  // calls StartThread() before StopThreadTask() runs, it's canceled by
  // invalidating its weak pointer. As a result we're guaranteed to only call
  // Thread::Stop() while there are no tasks on its queue. We don't try to stop
  // hung threads. But if it recovers it will be stopped the next time a client
  // calls this.
  for (size_t i = 0; i < threads_.size(); i++) {
    if (threads_[i]->thread.IsRunning() &&
        !threads_[i]->hang_detector.IsThreadLikelyHung()) {
      threads_[i]->thread.task_runner()->PostTaskAndReply(
          FROM_HERE, base::DoNothing(),
          base::BindOnce(&CodecAllocator::StopThreadTask,
                         weak_this_factory_.GetWeakPtr(), i));
    }
  }
}

// Return the task runner for tasks of type |type|.
scoped_refptr<base::SingleThreadTaskRunner> CodecAllocator::TaskRunnerFor(
    TaskType task_type) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return threads_[task_type]->thread.task_runner();
}

std::unique_ptr<MediaCodecBridge> CodecAllocator::CreateMediaCodecSync(
    scoped_refptr<CodecConfig> codec_config) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  auto task_type =
      TaskTypeForAllocation(codec_config->software_codec_forbidden);
  if (!task_type)
    return nullptr;

  auto codec = CreateMediaCodecInternal(factory_cb_, codec_config,
                                        task_type == SW_CODEC);
  if (codec)
    codec_task_types_[codec.get()] = *task_type;
  return codec;
}

void CodecAllocator::CreateMediaCodecAsync(
    base::WeakPtr<CodecAllocatorClient> client,
    scoped_refptr<CodecConfig> codec_config) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // We need to be ordered with respect to any Start/StopThread from this
    // client.  Otherwise, we might post work to the worker thread before the
    // posted task to start the worker threads (on |task_runner_|) has run yet.
    // We also need to avoid data races, since our member variables are all
    // supposed to be accessed from the main thread only.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CodecAllocator::CreateMediaCodecAsyncInternal,
                       base::Unretained(this),
                       base::ThreadTaskRunnerHandle::Get(), client,
                       codec_config));
    return;
  }

  // We're on the right thread, so just send in |task_runner_|.
  CreateMediaCodecAsyncInternal(task_runner_, client, codec_config);
}

void CodecAllocator::CreateMediaCodecAsyncInternal(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    base::WeakPtr<CodecAllocatorClient> client,
    scoped_refptr<CodecConfig> codec_config) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(client_task_runner);

  // TODO(liberato): BindOnce more often if possible.

  // Allocate the codec on the appropriate thread, and reply to this one with
  // the result.  If |client| is gone by then, we handle cleanup.
  auto task_type =
      TaskTypeForAllocation(codec_config->software_codec_forbidden);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      task_type ? TaskRunnerFor(*task_type) : nullptr;
  if (!task_type || !task_runner) {
    // The allocator threads didn't start or are stuck.
    // Post even if it's the current thread, to avoid re-entrancy.
    client_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&CodecAllocatorClient::OnCodecConfigured, client,
                       nullptr, codec_config->surface_bundle));
    return;
  }

  base::PostTaskAndReplyWithResult(
      task_runner.get(), FROM_HERE,
      base::BindOnce(&CreateMediaCodecInternal, factory_cb_, codec_config,
                     task_type == SW_CODEC),
      base::BindOnce(&CodecAllocator::ForwardOrDropCodec,
                     base::Unretained(this), client_task_runner, client,
                     *task_type, codec_config->surface_bundle));
}

void CodecAllocator::ForwardOrDropCodec(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    base::WeakPtr<CodecAllocatorClient> client,
    TaskType task_type,
    scoped_refptr<AVDASurfaceBundle> surface_bundle,
    std::unique_ptr<MediaCodecBridge> media_codec) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Remember: we are not necessarily on the right thread to use |client|.

  if (media_codec)
    codec_task_types_[media_codec.get()] = task_type;

  // We could call directly if |task_runner_| is the current thread.  Also note
  // that there's no guarantee that |client_task_runner|'s thread is still
  // running.  That's okay; MediaCodecAndSurface will handle it.
  client_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CodecAllocator::ForwardOrDropCodecOnClientThread,
                     base::Unretained(this), client,
                     std::make_unique<MediaCodecAndSurface>(
                         std::move(media_codec), std::move(surface_bundle))));
}

void CodecAllocator::ForwardOrDropCodecOnClientThread(
    base::WeakPtr<CodecAllocatorClient> client,
    std::unique_ptr<MediaCodecAndSurface> codec_and_surface) {
  // Note that if |client| has been destroyed, MediaCodecAndSurface will clean
  // up properly on the correct thread.  Also note that |surface_bundle| will be
  // preserved at least as long as the codec.
  if (!client)
    return;

  client->OnCodecConfigured(std::move(codec_and_surface->media_codec),
                            std::move(codec_and_surface->surface_bundle));
}

CodecAllocator::MediaCodecAndSurface::MediaCodecAndSurface(
    std::unique_ptr<MediaCodecBridge> codec,
    scoped_refptr<AVDASurfaceBundle> surface)
    : media_codec(std::move(codec)), surface_bundle(std::move(surface)) {}

CodecAllocator::MediaCodecAndSurface::~MediaCodecAndSurface() {
  // This code may be run on any thread.

  if (!media_codec)
    return;

  // If there are no registered clients, then the threads are stopped or are
  // stopping.  We must restart them / cancel any pending stop requests before
  // we can post codec destruction to them.  In the "restart them" case, the
  // threads aren't running.  In the "cancel...requests" case, the threads are
  // running, but we're trying to clear them out via a DoNothing task posted
  // there.  Once that completes, there will be a join on the main thread.  If
  // we post, then it will be ordered after the DoNothing, but before the join
  // on the main thread (this thread).  If the destruction task hangs, then so
  // will the join.
  //
  // We register a fake client to make sure that the threads are ready.
  //
  // If we can't start the thread, then ReleaseMediaCodec will free it on the
  // current thread.
  CodecAllocator* allocator = GetInstance(nullptr);
  allocator->StartThread(nullptr);
  allocator->ReleaseMediaCodec(std::move(media_codec),
                               std::move(surface_bundle));

  // We can stop the threads immediately.  If other clients are around, then
  // this will do nothing.  Otherwise, this will order the join after the
  // release completes successfully.
  allocator->StopThread(nullptr);
}

void CodecAllocator::ReleaseMediaCodec(
    std::unique_ptr<MediaCodecBridge> media_codec,
    scoped_refptr<AVDASurfaceBundle> surface_bundle) {
  DCHECK(media_codec);

  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // See CreateMediaCodecAsync
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CodecAllocator::ReleaseMediaCodec,
                       base::Unretained(this), std::move(media_codec),
                       std::move(surface_bundle)));
    return;
  }

  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  auto task_type = codec_task_types_[media_codec.get()];
  int erased = codec_task_types_.erase(media_codec.get());
  DCHECK(erased);

  // Save a waitable event for the release if the codec is attached to an
  // overlay so we can block on it in WaitForPendingReleaseForTesting().
  base::WaitableEvent* released_event = nullptr;
  if (surface_bundle->overlay) {
    pending_codec_releases_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(surface_bundle->overlay.get()),
        std::forward_as_tuple(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED));
    released_event =
        &pending_codec_releases_.find(surface_bundle->overlay.get())->second;
  }

  // Note that we forward |surface_bundle|, too, so that the surface outlasts
  // the codec.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      TaskRunnerFor(task_type);
  if (!task_runner) {
    // Thread isn't running, so just delete it now and hope for the best.
    DeleteMediaCodecAndSignal(std::move(media_codec), nullptr);
    OnMediaCodecReleased(std::move(surface_bundle));
    return;
  }

  task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DeleteMediaCodecAndSignal, std::move(media_codec),
                     released_event),
      base::BindOnce(&CodecAllocator::OnMediaCodecReleased,
                     base::Unretained(this), std::move(surface_bundle)));
}

void CodecAllocator::OnMediaCodecReleased(
    scoped_refptr<AVDASurfaceBundle> surface_bundle) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // This is a no-op if it's a non overlay bundle.
  pending_codec_releases_.erase(surface_bundle->overlay.get());
}

bool CodecAllocator::IsAnyRegisteredAVDA() {
  return !clients_.empty();
}

base::Optional<TaskType> CodecAllocator::TaskTypeForAllocation(
    bool software_codec_forbidden) {
  if (!threads_[AUTO_CODEC]->hang_detector.IsThreadLikelyHung())
    return AUTO_CODEC;

  if (!threads_[SW_CODEC]->hang_detector.IsThreadLikelyHung() &&
      !software_codec_forbidden) {
    return SW_CODEC;
  }

  return base::nullopt;
}

base::Thread& CodecAllocator::GetThreadForTesting(TaskType task_type) {
  return threads_[task_type]->thread;
}

bool CodecAllocator::WaitForPendingReleaseForTesting(AndroidOverlay* overlay) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!pending_codec_releases_.count(overlay))
    return true;

  // The codec is being released so we have to wait for it here. It's a
  // TimedWait() because the MediaCodec release may hang due to framework bugs.
  // And in that case we don't want to hang the browser UI thread. Android ANRs
  // occur when the UI thread is blocked for 5 seconds, so waiting for 2 seconds
  // gives us leeway to avoid an ANR. Verified no ANR on a Nexus 7.
  base::WaitableEvent& released = pending_codec_releases_.find(overlay)->second;
  released.TimedWait(base::TimeDelta::FromSeconds(2));
  if (released.IsSignaled())
    return true;

  DLOG(WARNING) << __func__ << ": timed out waiting for MediaCodec#release()";
  return false;
}

CodecAllocator::CodecAllocator(
    CodecAllocator::CodecFactoryCB factory_cb,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::TickClock* tick_clock,
    base::WaitableEvent* stop_event)
    : task_runner_(task_runner),
      stop_event_for_testing_(stop_event),
      factory_cb_(std::move(factory_cb)),
      weak_this_factory_(this) {
  // We leak the clock we create, but that's okay because we're a singleton.
  auto* clock = tick_clock ? tick_clock : base::DefaultTickClock::GetInstance();

  // Create threads with names and indices that match up with TaskType.
  threads_.push_back(new ThreadAndHangDetector("AVDAAutoThread", clock));
  threads_.push_back(new ThreadAndHangDetector("AVDASWThread", clock));
  static_assert(AUTO_CODEC == 0 && SW_CODEC == 1,
                "TaskType values are not ordered correctly.");
}

CodecAllocator::~CodecAllocator() {
  // Only tests should reach here.  Shut down threads so that we guarantee that
  // nothing will use the threads.
  for (auto* thread : threads_)
    thread->thread.Stop();
}

void CodecAllocator::StopThreadTask(size_t index) {
  threads_[index]->thread.Stop();
  // Signal the stop event after both threads are stopped.
  if (stop_event_for_testing_ && !threads_[AUTO_CODEC]->thread.IsRunning() &&
      !threads_[SW_CODEC]->thread.IsRunning()) {
    stop_event_for_testing_->Signal();
  }
}

}  // namespace media
