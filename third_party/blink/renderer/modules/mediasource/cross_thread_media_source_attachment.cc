// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/cross_thread_media_source_attachment.h"

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/mediasource/attachment_creation_pass_key_provider.h"
#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_public.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

// For debug logging a TrackAddRemovalType.
std::ostream& operator<<(
    std::ostream& stream,
    CrossThreadMediaSourceAttachment::TrackAddRemovalType track_type) {
  return stream << (track_type == CrossThreadMediaSourceAttachment::
                                      TrackAddRemovalType::kAudio
                        ? "audio"
                        : "video");
}

}  // anonymous namespace

CrossThreadMediaSourceAttachment::CrossThreadMediaSourceAttachment(
    MediaSource* media_source,
    AttachmentCreationPassKeyProvider::PassKey /* passkey */)
    : registered_media_source_(media_source),
      // To use scheduling priority that is the same as postMessage, we use
      // kPostedMessage here instead of, say, kMediaElementEvent.
      worker_runner_(media_source->GetExecutionContext()->GetTaskRunner(
          TaskType::kPostedMessage)),
      media_source_context_destroyed_(false),
      media_element_context_destroyed_(false),
      have_ever_attached_(false),
      have_ever_started_closing_(false) {
  // This kind of attachment can only be constructed by the worker thread.
  DCHECK(!IsMainThread());
  DCHECK(worker_runner_->BelongsToCurrentThread());

  DVLOG(1) << __func__ << " this=" << this << " media_source=" << media_source;

  // Verify that at construction time, refcounting of this object begins at
  // precisely 1.
  DCHECK(HasOneRef());
}

CrossThreadMediaSourceAttachment::~CrossThreadMediaSourceAttachment() {
  DVLOG(1) << __func__ << " this=" << this;
}

void CrossThreadMediaSourceAttachment::NotifyDurationChanged(
    MediaSourceTracer* /* tracer */,
    double new_duration) {
  DVLOG(1) << __func__ << " this=" << this << ", new_duration=" << new_duration;

  attachment_state_lock_.AssertAcquired();

  // Must only occur while attachment is "alive".
  VerifyCalledWhileContextsAliveForDebugging();

  // Called only by the MSE API on worker thread.
  DCHECK(!IsMainThread());
  DCHECK(worker_runner_->BelongsToCurrentThread());

  // Changing the duration has side effect of potentially new values for the
  // buffered and seekable ranges. Furthermore, duration-changed notification of
  // the media element, when attached cross-thread, needs to be done in a posted
  // task's dispatch (to both mitigate high-res timer creation by apps, and to
  // not require locks in the media element itself.) Finally, when that task is
  // executed on the main thread, the way of updating the media element's
  // duration correctly depends upon the element's current readyState. In short,
  // send updated values and indicate that the recipient on the main thread also
  // needs to correctly update the element's duration.
  SendUpdatedInfoToMainThreadCacheInternal(/* has_duration */ true,
                                           new_duration);
}

base::TimeDelta CrossThreadMediaSourceAttachment::GetRecentMediaTime(
    MediaSourceTracer* /* tracer */) {
  attachment_state_lock_.AssertAcquired();

  // Must only occur while attachment is "alive".
  VerifyCalledWhileContextsAliveForDebugging();

  // Called only by the MSE API on worker thread.
  DCHECK(!IsMainThread());
  DCHECK(worker_runner_->BelongsToCurrentThread());

  DVLOG(1) << __func__ << " this=" << this << "->" << recent_element_time_;

  return recent_element_time_;
}

bool CrossThreadMediaSourceAttachment::GetElementError(
    MediaSourceTracer* /* tracer */) {
  attachment_state_lock_.AssertAcquired();

  // Must only occur while attachment is "alive".
  VerifyCalledWhileContextsAliveForDebugging();

  // Called only by the MSE API on worker thread.
  DCHECK(!IsMainThread());
  DCHECK(worker_runner_->BelongsToCurrentThread());

  DVLOG(1) << __func__ << " this=" << this << "->" << element_has_error_;

  return element_has_error_;
}

AudioTrackList* CrossThreadMediaSourceAttachment::CreateAudioTrackList(
    MediaSourceTracer* /* tracer */) {
  // TODO(https://crbug.com/878133): Implement this once worker thread can
  // create track lists.
  NOTIMPLEMENTED();
  return nullptr;
}

VideoTrackList* CrossThreadMediaSourceAttachment::CreateVideoTrackList(
    MediaSourceTracer* /* tracer */) {
  // TODO(https://crbug.com/878133): Implement this once worker thread can
  // create track lists.
  NOTIMPLEMENTED();
  return nullptr;
}

void CrossThreadMediaSourceAttachment::AddAudioTrackToMediaElement(
    MediaSourceTracer* /* tracer */,
    AudioTrack* /* track */) {
  // TODO(https://crbug.com/878133): Implement this once worker thread can
  // create tracks.
  NOTIMPLEMENTED();
}

void CrossThreadMediaSourceAttachment::AddVideoTrackToMediaElement(
    MediaSourceTracer* /* tracer */,
    VideoTrack* /* track */) {
  // TODO(https://crbug.com/878133): Implement this once worker thread can
  // create tracks.
  NOTIMPLEMENTED();
}

void CrossThreadMediaSourceAttachment::RemoveAudioTracksFromMediaElement(
    MediaSourceTracer* /* tracer */,
    Vector<String> audio_ids,
    bool enqueue_change_event) {
  DVLOG(1) << __func__ << " this=" << this
           << " audio_ids size()=" << audio_ids.size()
           << ", enqueue_change_event=" << enqueue_change_event;
  attachment_state_lock_.AssertAcquired();
  RemoveTracksFromMediaElementInternal(TrackAddRemovalType::kAudio,
                                       std::move(audio_ids),
                                       std::move(enqueue_change_event));
}

void CrossThreadMediaSourceAttachment::RemoveVideoTracksFromMediaElement(
    MediaSourceTracer* /* tracer */,
    Vector<String> video_ids,
    bool enqueue_change_event) {
  DVLOG(1) << __func__ << " this=" << this
           << " video_ids size()=" << video_ids.size()
           << ", enqueue_change_event=" << enqueue_change_event;
  attachment_state_lock_.AssertAcquired();
  RemoveTracksFromMediaElementInternal(TrackAddRemovalType::kVideo,
                                       std::move(video_ids),
                                       std::move(enqueue_change_event));
}

void CrossThreadMediaSourceAttachment::RemoveTracksFromMediaElementInternal(
    TrackAddRemovalType track_type,
    Vector<String> track_ids,
    bool enqueue_change_event) {
  attachment_state_lock_.AssertAcquired();

  // Called only by the MSE API on worker thread.
  DCHECK(!IsMainThread());
  DCHECK(worker_runner_->BelongsToCurrentThread());

  DCHECK(!track_ids.empty());

  // Detachment might have started, and could lead to MSE teardown calling us.
  // In such case, media element must forget all its tracks directly (see
  // various calls in HTMLMediaElement to ForgetResourceSpecificTracks() that
  // occur in states where an existing MSE attachment is closing), and we can
  // return early here. This also avoids removing tracks from the media element
  // which may have already begun loading from a different source.
  if (have_ever_started_closing_) {
    DVLOG(1) << __func__ << " this=" << this << ", track_type=" << track_type
             << ": media element has begun detachment (::Close). no-op";
    return;
  }

  if (media_element_context_destroyed_) {
    DVLOG(1) << __func__ << " this=" << this << ", track_type=" << track_type
             << ": media element context is destroyed. no-op";
    return;
  }

  DCHECK(main_runner_);
  DCHECK(attached_element_);

  // Otherwise, post a task to the main thread to update the media element's
  // track lists there. Note that task might never run if the main context is
  // destroyed in the interim. Using WTF::RetainedRef(this) here to ensure we
  // are still alive if/when |main_runner_| executes the task. Note that there
  // exists a CrossThreadCopier<Vector<String>> that deep-copies the bound ids
  // vector to keep thread safety for the contained Strings.
  PostCrossThreadTask(
      *main_runner_, FROM_HERE,
      CrossThreadBindOnce(&CrossThreadMediaSourceAttachment::
                              RemoveTracksFromMediaElementOnMainThread,
                          WTF::RetainedRef(this), track_type, track_ids,
                          enqueue_change_event));
}

void CrossThreadMediaSourceAttachment::RemoveTracksFromMediaElementOnMainThread(
    TrackAddRemovalType track_type,
    Vector<String> track_ids,
    bool enqueue_change_event) {
  DCHECK(!track_ids.empty());

  {
    base::AutoLock lock(attachment_state_lock_);

    DCHECK(IsMainThread());
    DCHECK(main_runner_->BelongsToCurrentThread());

    DVLOG(1) << __func__ << " this=" << this << ", track_type=" << track_type
             << ", track_ids size()=" << track_ids.size()
             << ", enqueue_change_event=" << enqueue_change_event;

    // While awaiting task scheduling, media element could have begun
    // detachment or main context could have been destroyed. Return early in
    // these cases. See RemoveTracksFromMediaElementInternal() for explanation.
    if (have_ever_started_closing_) {
      DVLOG(1) << __func__ << " this=" << this << ", track_type=" << track_type
               << ": media element has begun detachment (::Close). no-op";
      return;
    }

    if (media_element_context_destroyed_) {
      DVLOG(1) << __func__ << " this=" << this << ", track_type=" << track_type
               << ": media element context is destroyed: no-op";
      return;
    }

    // Detection of |have_ever_started_closing_|, above, should prevent this
    // from failing.
    DCHECK(attached_element_);

    for (const String& id : track_ids) {
      switch (track_type) {
        case TrackAddRemovalType::kAudio:
          attached_element_->audioTracks().Remove(id);
          break;
        case TrackAddRemovalType::kVideo:
          attached_element_->videoTracks().Remove(id);
          break;
      }
    }

    if (enqueue_change_event) {
      Event* event = Event::Create(event_type_names::kChange);
      switch (track_type) {
        case TrackAddRemovalType::kAudio:
          event->SetTarget(&attached_element_->audioTracks());
          break;
        case TrackAddRemovalType::kVideo:
          event->SetTarget(&attached_element_->videoTracks());
          break;
      }
      attached_element_->ScheduleEvent(event);
    }

    DVLOG(1) << __func__ << " this=" << this << ", track_type=" << track_type
             << ": done";
  }
}

void CrossThreadMediaSourceAttachment::AddMainThreadAudioTrackToMediaElement(
    String id,
    String kind,
    String label,
    String language,
    bool enabled) {
  DVLOG(1) << __func__ << " this=" << this << ", id=" << id << ", kind=" << kind
           << ", label=" << label << ", language=" << language
           << ", enabled=" << enabled;
  attachment_state_lock_.AssertAcquired();
  AddTrackToMediaElementInternal(TrackAddRemovalType::kAudio, std::move(id),
                                 std::move(kind), std::move(label),
                                 std::move(language), enabled);
}

void CrossThreadMediaSourceAttachment::AddMainThreadVideoTrackToMediaElement(
    String id,
    String kind,
    String label,
    String language,
    bool selected) {
  DVLOG(1) << __func__ << " this=" << this << ", id=" << id << ", kind=" << kind
           << ", label=" << label << ", language=" << language
           << ", selected=" << selected;
  attachment_state_lock_.AssertAcquired();
  AddTrackToMediaElementInternal(TrackAddRemovalType::kVideo, std::move(id),
                                 std::move(kind), std::move(label),
                                 std::move(language), selected);
}

void CrossThreadMediaSourceAttachment::AddTrackToMediaElementInternal(
    TrackAddRemovalType track_type,
    String id,
    String kind,
    String label,
    String language,
    bool enable_or_select) {
  attachment_state_lock_.AssertAcquired();

  // Called only by the MSE API on worker thread.
  DCHECK(!IsMainThread());
  DCHECK(worker_runner_->BelongsToCurrentThread());

  VerifyCalledWhileContextsAliveForDebugging();
  DCHECK(main_runner_);

  // Post a task to the main thread to add the track to the appropriate media
  // element track list there. Note that task might never run if the main
  // context is destroyed in the interim. Using WTF::RetainedRef(this) here to
  // ensure we are still alive if/when |main_runner_| executes the task. Note
  // that there exists a CrossThreadCopier<String> that deep-copies the bound
  // Strings to keep thread safety.
  PostCrossThreadTask(
      *main_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &CrossThreadMediaSourceAttachment::AddTrackToMediaElementOnMainThread,
          WTF::RetainedRef(this), track_type, id, kind, label, language,
          enable_or_select));
}

void CrossThreadMediaSourceAttachment::AddTrackToMediaElementOnMainThread(
    TrackAddRemovalType track_type,
    String id,
    String kind,
    String label,
    String language,
    bool enable_or_select) {
  base::AutoLock lock(attachment_state_lock_);

  DCHECK(IsMainThread());
  DCHECK(main_runner_->BelongsToCurrentThread());

  DVLOG(1) << __func__ << " this=" << this << ", track_type=" << track_type
           << ", id=" << id << ", kind=" << kind << ", label=" << label
           << ", language=" << language
           << ", enable_or_select=" << enable_or_select;

  // While awaiting task scheduling, media element could have begun detachment
  // or main context could have been destroyed. Return early in these cases. See
  // RemoveTracksFromMediaElementInternal() for explanation.
  if (have_ever_started_closing_) {
    DVLOG(1) << __func__ << " this=" << this << ", track_type=" << track_type
             << ": media element has begun detachment (::Close). no-op";
    return;
  }

  if (media_element_context_destroyed_) {
    DVLOG(1) << __func__ << " this=" << this << ", track_type=" << track_type
             << ": media element context is destroyed: no-op";
    return;
  }

  // Detection of |have_ever_started_closing_|, above, should prevent this from
  // failing.
  DCHECK(attached_element_);

  // Create and add the appropriate main-thread-owned track to the attached
  // media element. Note, we use default nullptr for the supplemental
  // sourceBuffer attribute to prevent main thread JS from attempting to
  // reference worker-thread SourceBuffer. Due to lack of deducible conversion
  // from WTF::String to WTF::AtomicString, we construct the atomics locally for
  // use in track creation here.
  const AtomicString atomic_id(id);
  const AtomicString atomic_kind(kind);
  const AtomicString atomic_label(label);
  const AtomicString atomic_language(language);
  switch (track_type) {
    case TrackAddRemovalType::kAudio: {
      auto* audio_track =
          MakeGarbageCollected<AudioTrack>(atomic_id, atomic_kind, atomic_label,
                                           atomic_language, enable_or_select,
                                           /*exclusive=*/false);
      attached_element_->audioTracks().Add(audio_track);
      break;
    }
    case TrackAddRemovalType::kVideo: {
      auto* video_track =
          MakeGarbageCollected<VideoTrack>(atomic_id, atomic_kind, atomic_label,
                                           atomic_language, enable_or_select);
      attached_element_->videoTracks().Add(video_track);
      break;
    }
  }

  DVLOG(1) << __func__ << " this=" << this << ", track_type=" << track_type
           << ": done";
}

void CrossThreadMediaSourceAttachment::OnMediaSourceContextDestroyed() {
  attachment_state_lock_.AssertAcquired();

  // Called only by the MSE API on worker thread.
  DCHECK(!IsMainThread());
  DCHECK(worker_runner_->BelongsToCurrentThread());

  DVLOG(1) << __func__ << " this=" << this;

  // We shouldn't be notified more than once.
  DCHECK(!media_source_context_destroyed_);
  media_source_context_destroyed_ = true;
}

bool CrossThreadMediaSourceAttachment::FullyAttachedOrSameThread(
    SourceBufferPassKey) const {
  attachment_state_lock_.AssertAcquired();

  // We must only be used by the MSE API on the worker thread.
  DCHECK(!IsMainThread());
  DCHECK(worker_runner_->BelongsToCurrentThread());

  // We might be called while MSE worker context is being destroyed, but we must
  // not be called if we've never been used yet to attach.
  DCHECK(attached_media_source_);
  DCHECK(have_ever_attached_);

  return !media_element_context_destroyed_ && attached_element_ &&
         !have_ever_started_closing_;
}

bool CrossThreadMediaSourceAttachment::RunExclusively(
    bool abort_if_not_fully_attached,
    RunExclusivelyCB cb) {
  base::AutoLock lock(attachment_state_lock_);

  // We must only be used by the MSE API on the worker thread.
  DCHECK(!IsMainThread());
  DCHECK(worker_runner_->BelongsToCurrentThread());

  // We must never be called if the MSE API context has already destructed.
  DCHECK(!media_source_context_destroyed_);
  DCHECK(attached_media_source_);
  DCHECK(have_ever_attached_);

  if (abort_if_not_fully_attached &&
      (media_element_context_destroyed_ || !attached_element_ ||
       have_ever_started_closing_)) {
    DVLOG(1) << __func__ << " this=" << this
             << ", aborting due to not currently being fully attached";
    return false;
  }

  DVLOG(1) << __func__ << " this=" << this
           << ", running the callback with attachment state lock held";
  std::move(cb).Run(GetExclusiveKey());
  return true;
}

void CrossThreadMediaSourceAttachment::Unregister() {
  // MSE-in-Worker does NOT use object URLs, so this should not be called.
  NOTREACHED();
}

MediaSourceTracer*
CrossThreadMediaSourceAttachment::StartAttachingToMediaElement(
    HTMLMediaElement* element,
    bool* success) {
  DVLOG(1) << __func__ << " this=" << this << ", element=" << element;

  // Called only by the media element on main thread.
  DCHECK(IsMainThread());

  DCHECK(element);
  DCHECK(success);

  {
    base::AutoLock lock(attachment_state_lock_);

    // There should not be the ability for a previous (or current) element's
    // context to have been destroyed (main thread), yet us being called again
    // here, now. If this assumption is false and we reach here, debugging and
    // fixing will be necessary. Ensure we strongly enforce the assumption.
    CHECK(!media_element_context_destroyed_);

    // Prevent sequential re-use of this attachment for multiple successful
    // attachments. See declaration of |have_ever_attached_|.
    if (have_ever_attached_) {
      // With current restrictions on ability to only ever obtain at most one
      // MediaSourceHandle per MediaSource and only allow loading to succeed
      // at most once per each MediaSourceHandle, fail if there is attempt to
      // reuse either in a load.
      DVLOG(1) << __func__ << " this=" << this << ", element=" << element
               << ": failed: reuse of MediaSource for more than one load "
                  "is not supported for MSE-in-Workers";
      *success = false;
      return nullptr;
    }

    // If we've never been successfully attached, then we must never have
    // started closing.
    DCHECK(!have_ever_started_closing_);

    // Likewise, we must never have been able to receive the worker context
    // destruction notification if we've never been successfully attached.
    DCHECK(!media_source_context_destroyed_);

    // Fail if already unregistered. This should be rare: caller's retrieval of
    // this attachment instance is done by finding us in the registry. Probably
    // the only reason we might now be unregistered would be an intervening
    // action (like explicit revocation) occurrring on the worker thread.
    // (Worker's context destruction could also cause this, but we've already
    // checked this, above.)
    if (!registered_media_source_) {
      DVLOG(1) << __func__ << " this=" << this << ", element=" << element
               << ": failed: unregistered already";
      *success = false;
      return nullptr;
    }

    // Fail if this attachment is still attached. Since |have_ever_attached_|
    // check, above, prevents this case, this is explicitly a DCHECK instead of
    // an allowed failure.
    DCHECK(!attached_element_);
    DCHECK(!attached_media_source_);

    // On the *main* thread, synchronously attempt to begin attachment to the
    // worker-owned MediaSource. Operations like this require the MediaSource
    // and SourceBuffer to use RunExclusively for many of their normal
    // operations to ensure thread-safety.
    *success =
        registered_media_source_->StartWorkerAttachingToMainThreadMediaElement(
            WrapRefCounted(this));
    if (!*success) {
      DVLOG(1)
          << __func__ << " this=" << this << ", element=" << element
          << ": failed: MediaSource possibly in use via another attachment";
      return nullptr;
    }

    attached_element_ = element;
    attached_media_source_ = registered_media_source_;
    main_runner_ =
        element->GetExecutionContext()->GetTaskRunner(TaskType::kPostedMessage);
    DCHECK(main_runner_->BelongsToCurrentThread());

    // Before element starts pumping time and error status to us, use its
    // current status initially.
    recent_element_time_ = base::Seconds(element->currentTime());
    element_has_error_ = !!element->error();

    // Media element should not call this method if it already has an error.
    DCHECK(!element_has_error_);

    have_ever_attached_ = true;

    VerifyCalledWhileContextsAliveForDebugging();

    return nullptr;  // MediaSourceTracer is not used by cross-thread attach.
  }
}

void CrossThreadMediaSourceAttachment::CompleteAttachingToMediaElement(
    MediaSourceTracer* /* tracer */,
    std::unique_ptr<WebMediaSource> web_media_source) {
  DVLOG(1) << __func__ << " this=" << this
           << ", web_media_source=" << web_media_source.get();
  DCHECK(web_media_source);

  {
    base::AutoLock lock(attachment_state_lock_);

    // Called only by the media element on main thread.
    DCHECK(IsMainThread());
    DCHECK(main_runner_->BelongsToCurrentThread());

    // Media element must not call us if it has already received context
    // destruction notification. Ensure we strongly enforce the assumption here
    // to increase confidence in safety.
    CHECK(!media_element_context_destroyed_);

    // We must have succeeded with StartAttachingToMediaElement().
    DCHECK(have_ever_attached_);
    DCHECK(worker_runner_);

    // In unlikely case this attachment is reused, clear the cached state of
    // previous attachment.
    cached_buffered_.clear();
    cached_seekable_.clear();

    // Verify the rest of the status once we're completing this in the worker
    // thread. Using WTF::RetainedRef(this) here to ensure we are still alive
    // if/when |worker_runner_| executes the task.
    PostCrossThreadTask(
        *worker_runner_, FROM_HERE,
        CrossThreadBindOnce(&CrossThreadMediaSourceAttachment::
                                CompleteAttachingToMediaElementOnWorkerThread,
                            WTF::RetainedRef(this),
                            std::move(web_media_source)));
  }
}

void CrossThreadMediaSourceAttachment::
    CompleteAttachingToMediaElementOnWorkerThread(
        std::unique_ptr<WebMediaSource> web_media_source) {
  DCHECK(web_media_source);

  {
    base::AutoLock lock(attachment_state_lock_);

    DCHECK(!IsMainThread());
    DCHECK(worker_runner_->BelongsToCurrentThread());

    DVLOG(1) << __func__ << " this=" << this
             << ", web_media_source=" << web_media_source.get();

    // While awaiting task scheduling, the media element could have begun
    // detachment or main context could have been destroyed. Return early in
    // these cases.
    if (have_ever_started_closing_) {
      DVLOG(1) << __func__ << " this=" << this
               << ": media element has begun detachment. no-op";
      return;
    }
    if (media_element_context_destroyed_) {
      DVLOG(1) << __func__ << " this=" << this
               << ": media element context is destroyed. no-op";
      return;
    }

    // Before or while awaiting task scheduling, the worker context could have
    // been destroyed.
    if (media_source_context_destroyed_) {
      // TODO(https://crbug.com/878133): Determine how to specify notification
      // of a "defunct" worker-thread MediaSource in the case where it was
      // serving as the source for a media element. Directly notifying an error
      // via the |web_media_source_| may be the appropriate route here, but
      // MarkEndOfStream internally has constraints (already initialized
      // demuxer, not already "ended", etc) which make it unsuitable currently
      // for this purpose. Currently, we prevent further usage of the underlying
      // demuxer and return sane values to the element for its queries (nothing
      // buffered, nothing seekable) once the attached  media source's context
      // is destroyed.
      return;
    }

    VerifyCalledWhileContextsAliveForDebugging();

    attached_media_source_->CompleteAttachingToMediaElement(
        std::move(web_media_source));
  }
}

void CrossThreadMediaSourceAttachment::Close(MediaSourceTracer* /* tracer */) {
  base::AutoLock lock(attachment_state_lock_);
  DVLOG(1) << __func__ << " this=" << this;

  // Note, this method may be called either explicitly to detach a loading
  // MediaSource, or to detach upon media element context destruction. So we
  // cannot make any assumption about the availability of the media element in
  // the scope of this call (nor in the posted task to the worker thread).
  // Called only by the media element on main thread.
  DCHECK(IsMainThread());
  DCHECK(main_runner_->BelongsToCurrentThread());

  // Should be called once at most.
  DCHECK(!have_ever_started_closing_);
  have_ever_started_closing_ = true;

  // We must have previously succeeded with StartAttachingToMediaElement().
  DCHECK(have_ever_attached_);
  DCHECK(worker_runner_);

  // Verify the rest of the status once we're completing the close in the
  // worker thread. Meanwhile, |have_ever_started_closing_| will prevent usage
  // of the underlying WebMediaSource and WebSourceBuffer (see
  // RunExclusively()), since the Chromium abstractions underlying those are
  // owned by the main thread WebMediaPlayer which is shutting down concurrently
  // with the task scheduling to complete the close operation on the worker
  // thread. Using WTF::RetainedRef(this) here to ensure we are still alive
  // if/when |worker_runner_| executes the task.
  PostCrossThreadTask(
      *worker_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &CrossThreadMediaSourceAttachment::CloseOnWorkerThread,
          WTF::RetainedRef(this)));
}

void CrossThreadMediaSourceAttachment::CloseOnWorkerThread() {
  base::AutoLock lock(attachment_state_lock_);

  DCHECK(!IsMainThread());
  DCHECK(worker_runner_->BelongsToCurrentThread());
  DCHECK(have_ever_started_closing_);
  DVLOG(1) << __func__ << " this=" << this;

  if (media_source_context_destroyed_) {
    // Conditional notification of error via the pipeline may have already been
    // initiated. Regardless, we have no further work to do here.
    DVLOG(1) << __func__ << " this=" << this
             << ": worker context is destroyed. no-op";
    return;
  }

  // While awaiting task scheduling, the main context could have been destroyed.
  if (media_element_context_destroyed_) {
    // TODO(https://crbug.com/878133): See comment around current lack of
    // notifying worker JS upon attached element's context destruction in this
    // experimental implementation. Regardless, the solution to that will likely
    // be to initiate some notification (not "Close") that cross-thread notifies
    // the MSE objects that their attachment is forcibly detached due to
    // destructed main context. So we do nothing here, anticipating such
    // solution, and also to protect against accessing the underlying
    // WebMediaSource/WebSourceBuffers when the  main context is destroyed.
    DVLOG(1) << __func__ << " this=" << this
             << ": main context is destroyed. no-op";
    return;
  }

  DCHECK(attached_element_);
  DCHECK(attached_media_source_);
  attached_media_source_->Close();

  // Note, we will be destructed once both sides (HTMLME and MSE), along with
  // the registry, no longer have a reference to us. Each side does that, if
  // they are themselves alive, during: main element's scope of calling our
  // ::Close(), and worker MSE scope's Close(), above. When we're destructed,
  // this will remove the CrossThreadPersistent Oilpan root references to each
  // side, too, enabling their eventual GC. Further, if either context is
  // destroyed, then even before we're destructed, the appropriate
  // CrossThreadPersistent reference we have to that side's object is
  // automatically cleared by Oilpan.
}

WebTimeRanges CrossThreadMediaSourceAttachment::BufferedInternal(
    MediaSourceTracer* /* tracer */) const {
  base::AutoLock lock(attachment_state_lock_);

  // Called only by the media element on main thread.
  DCHECK(IsMainThread());
  DCHECK(main_runner_->BelongsToCurrentThread());

  // We shouldn't ever be called if the media element's context is destroyed.
  DCHECK(!media_element_context_destroyed_);
  DCHECK(attached_element_);

  // We shouldn't ever be called if the media element has already called our
  // Close() method.
  DCHECK(!have_ever_started_closing_);

  // We shouldn't ever be called if the media element has never successfully
  // started an attachment using us.
  DCHECK(have_ever_attached_);

  DVLOG(1) << __func__ << " this=" << this;
  // The worker context might have already been destroyed, but the media element
  // might not yet have realized there is error (that signal, if any, hops
  // threads).  Until there is resolution on spec behavior here (see
  // https://github.com/w3c/media-source/issues/277), override any cached value
  // and return an empty range here if the worker's context has been destroyed.
  if (media_source_context_destroyed_) {
    DVLOG(1) << __func__ << " this=" << this
             << ": worker context destroyed. Returning 'nothing buffered'";
    return {};
  }

  // In CrossThread attachments, we use kPostedMessage cross-thread task posting
  // to have similar information propagation latency and causality as an
  // application using postMessage() from worker to main thread. See
  // SendUpdatedInfoToMainThreadCache() for where the MSE API initiates updates
  // of the main thread's cache of buffered and seekable values.
  return cached_buffered_;
}

WebTimeRanges CrossThreadMediaSourceAttachment::SeekableInternal(
    MediaSourceTracer* /* tracer */) const {
  base::AutoLock lock(attachment_state_lock_);

  // Called only by the media element on main thread.
  DCHECK(IsMainThread());
  DCHECK(main_runner_->BelongsToCurrentThread());

  // We shouldn't ever be called if the media element's context is destroyed.
  DCHECK(!media_element_context_destroyed_);
  DCHECK(attached_element_);

  // We shouldn't ever be called if the media element has already called our
  // Close() method.
  DCHECK(!have_ever_started_closing_);

  // We shouldn't ever be called if the media element has never successfully
  // started an attachment using us.
  DCHECK(have_ever_attached_);

  DVLOG(1) << __func__ << " this=" << this;

  // The worker context might have already been destroyed, but the media element
  // might not yet have realized there is error (that signal, if any, hops
  // threads).  Until there is resolution on spec behavior here (see
  // https://github.com/w3c/media-source/issues/277), override any cached value
  // and return an empty range here if the worker's context has been destroyed.
  if (media_source_context_destroyed_) {
    DVLOG(1) << __func__ << " this=" << this
             << ": worker context destroyed. Returning 'nothing seekable'";
    return {};
  }

  // In CrossThread attachments, we use kPostedMessage cross-thread task posting
  // to have similar information propagation latency and causality as an
  // application using postMessage() from worker to main thread. See
  // SendUpdatedInfoToMainThreadCache() for where the MSE API initiates updates
  // of the main thread's cache of buffered and seekable values.
  return cached_seekable_;
}

void CrossThreadMediaSourceAttachment::OnTrackChanged(
    MediaSourceTracer* /* tracer */,
    TrackBase* track) {
  base::AutoLock lock(attachment_state_lock_);

  // Called only by the media element on main thread.
  DCHECK(IsMainThread());
  DCHECK(main_runner_->BelongsToCurrentThread());

  // TODO(https://crbug.com/878133): Implement cross-thread behavior for this
  // once worker thread can create tracks.
}

void CrossThreadMediaSourceAttachment::OnElementTimeUpdate(double time) {
  DVLOG(1) << __func__ << " this=" << this << ", time=" << time;

  {
    base::AutoLock lock(attachment_state_lock_);

    // Called only by the media element on main thread.
    DCHECK(IsMainThread());
    DCHECK(main_runner_->BelongsToCurrentThread());
    DCHECK(!media_element_context_destroyed_);

    // Post the updated info to the worker thread.
    DCHECK(worker_runner_);

    // TODO(https://crbug.com/878133): Consider coalescing frequent calls to
    // this using a timer.

    PostCrossThreadTask(
        *worker_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &CrossThreadMediaSourceAttachment::UpdateWorkerThreadTimeCache,
            WTF::RetainedRef(this), base::Seconds(time)));
  }
}

void CrossThreadMediaSourceAttachment::UpdateWorkerThreadTimeCache(
    base::TimeDelta time) {
  {
    base::AutoLock lock(attachment_state_lock_);
    DCHECK(!IsMainThread());
    DCHECK(worker_runner_->BelongsToCurrentThread());

    // Worker context might have destructed, and if so, we don't really need to
    // update the time since there should be no further reads of it. Similarly,
    // attached_media_source_ might have been cleared already (by Oilpan for
    // that CrossThreadPersistent on it's owning context's destruction).
    recent_element_time_ = time;

    DVLOG(1) << __func__ << " this=" << this
             << ": updated recent_element_time_=" << recent_element_time_;
  }
}

void CrossThreadMediaSourceAttachment::OnElementError() {
  DVLOG(1) << __func__ << " this=" << this;

  {
    base::AutoLock lock(attachment_state_lock_);

    // Called only by the media element on main thread.
    DCHECK(IsMainThread());
    DCHECK(main_runner_->BelongsToCurrentThread());
    DCHECK(!media_element_context_destroyed_);

    PostCrossThreadTask(
        *worker_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &CrossThreadMediaSourceAttachment::HandleElementErrorOnWorkerThread,
            WTF::RetainedRef(this)));
  }
}

void CrossThreadMediaSourceAttachment::HandleElementErrorOnWorkerThread() {
  base::AutoLock lock(attachment_state_lock_);
  DCHECK(!IsMainThread());
  DCHECK(worker_runner_->BelongsToCurrentThread());

  DCHECK(!element_has_error_)
      << "At most one transition to element error per attachment is expected";

  element_has_error_ = true;

  DVLOG(1) << __func__ << " this=" << this << ": error flag set";
}

void CrossThreadMediaSourceAttachment::OnElementContextDestroyed() {
  base::AutoLock lock(attachment_state_lock_);

  // Called only by the media element on main thread.
  DCHECK(IsMainThread());
  DCHECK(main_runner_->BelongsToCurrentThread());

  DVLOG(1) << __func__ << " this=" << this;

  // We shouldn't be notified more than once.
  DCHECK(!media_element_context_destroyed_);
  media_element_context_destroyed_ = true;

  // TODO(https://crbug.com/878133): Is there any notification/state change we
  // need to do to a potentially open MediaSource that might still be alive on
  // the worker context here? The assumption in this experimental CL is that
  // once the main context is destroyed, it will not be long until the MSE
  // worker context is also destroyed, since the main context is assumed to have
  // been the root/parent context that created the dedicated worker. Hence, no
  // notification of the JS in worker on main thread context destruction seems
  // safe. We could perhaps initiate a cross-thread Close operation here.
}

void CrossThreadMediaSourceAttachment::
    AssertCrossThreadMutexIsAcquiredForDebugging() {
  attachment_state_lock_.AssertAcquired();
}

void CrossThreadMediaSourceAttachment::SendUpdatedInfoToMainThreadCache() {
  // No explicit duration update was done by application in this case.
  SendUpdatedInfoToMainThreadCacheInternal(false, 0);
}

void CrossThreadMediaSourceAttachment::SendUpdatedInfoToMainThreadCacheInternal(
    bool has_new_duration,
    double new_duration) {
  attachment_state_lock_.AssertAcquired();
  VerifyCalledWhileContextsAliveForDebugging();
  DCHECK(main_runner_);

  DVLOG(1) << __func__ << " this=" << this;

  // Called only by the MSE API on worker thread.
  DCHECK(!IsMainThread());
  DCHECK(worker_runner_->BelongsToCurrentThread());

  // TODO(https://crbug.com/878133): Consider coalescing frequent calls to this
  // using a timer, except when |has_new_duration| is true.

  // Here, since we are in scope of |lock| holding |attachment_state_lock_|, we
  // can correctly acquire an ExclusiveKey to give to MediaSource so it can know
  // that it is safe to access the underlying demuxer.
  WebTimeRanges new_buffered =
      attached_media_source_->BufferedInternal(GetExclusiveKey());
  WebTimeRanges new_seekable =
      attached_media_source_->SeekableInternal(GetExclusiveKey());

  PostCrossThreadTask(
      *main_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &CrossThreadMediaSourceAttachment::UpdateMainThreadInfoCache,
          WTF::RetainedRef(this), std::move(new_buffered),
          std::move(new_seekable), has_new_duration, new_duration));
}

void CrossThreadMediaSourceAttachment::UpdateMainThreadInfoCache(
    WebTimeRanges new_buffered,
    WebTimeRanges new_seekable,
    bool has_new_duration,
    double new_duration) {
  base::AutoLock lock(attachment_state_lock_);

  DCHECK(IsMainThread());
  DCHECK(main_runner_->BelongsToCurrentThread());

  DVLOG(1) << __func__ << " this=" << this;

  // While awaiting task scheduling, media element could have begun detachment
  // or main context could have been destroyed. Return early in these cases.
  if (have_ever_started_closing_) {
    DVLOG(1) << __func__ << " this=" << this
             << ": media element has begun detachment (::Close). no-op";
    return;
  }

  if (media_element_context_destroyed_) {
    DVLOG(1) << __func__ << " this=" << this
             << ": media element context is destroyed: no-op";
    return;
  }

  // Detection of |have_ever_started_closing_|, above, should prevent this
  // from failing.
  DCHECK(attached_element_);

  cached_buffered_ = std::move(new_buffered);
  cached_seekable_ = std::move(new_seekable);

  if (has_new_duration) {
    // We may need to let the media element know duration has changed. Whether
    // we do this needs to be conditioned upon the media element's current
    // readyState.
    if (attached_element_->getReadyState() == HTMLMediaElement::kHaveNothing) {
      DVLOG(1) << __func__ << " this=" << this
               << ": new_duration=" << new_duration
               << " and media element readyState is HAVE_NOTHING";
      // Explicitly notify the media element of the updated duration. This
      // happens when app sets MediaSource duration before the pipeline has
      // reached HAVE_METADATA.
      bool request_seek = attached_element_->currentTime() > new_duration;
      attached_element_->DurationChanged(new_duration, request_seek);
      return;
    }

    // The pipeline will deliver explicit duration changed notifications to the
    // element at and after the transition to HAVE_METADATA, including when the
    // app sets MediaSource duration in those cases, so don't deliver any extra
    // notification to the element here.
    DVLOG(1) << __func__ << " this=" << this
             << ": new_duration=" << new_duration
             << " and media element readyState is beyond HAVE_NOTHING (no-op)";
  }
}

void CrossThreadMediaSourceAttachment::
    VerifyCalledWhileContextsAliveForDebugging() const {
  attachment_state_lock_.AssertAcquired();
  DCHECK(!media_source_context_destroyed_);
  DCHECK(!media_element_context_destroyed_);
  DCHECK(attached_element_);
  DCHECK(attached_media_source_);
  DCHECK(!have_ever_started_closing_);
  DCHECK(have_ever_attached_);
}

}  // namespace blink
