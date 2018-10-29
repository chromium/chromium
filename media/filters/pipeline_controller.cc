// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/pipeline_controller.h"

#include "base/bind.h"
#include "media/base/demuxer.h"

namespace media {

PipelineController::PipelineController(
    std::unique_ptr<Pipeline> pipeline,
    const RendererFactoryCB& renderer_factory_cb,
    const SeekedCB& seeked_cb,
    const SuspendedCB& suspended_cb,
    const BeforeResumeCB& before_resume_cb,
    const ResumedCB& resumed_cb,
    const PipelineStatusCB& error_cb)
    : pipeline_(std::move(pipeline)),
      renderer_factory_cb_(renderer_factory_cb),
      seeked_cb_(seeked_cb),
      suspended_cb_(suspended_cb),
      before_resume_cb_(before_resume_cb),
      resumed_cb_(resumed_cb),
      error_cb_(error_cb),
      weak_factory_(this) {
  DCHECK(pipeline_);
  DCHECK(renderer_factory_cb_);
  DCHECK(seeked_cb_);
  DCHECK(suspended_cb_);
  DCHECK(before_resume_cb_);
  DCHECK(resumed_cb_);
  DCHECK(error_cb_);
}

PipelineController::~PipelineController() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void PipelineController::Start(Pipeline::StartType start_type,
                               Demuxer* demuxer,
                               Pipeline::Client* client,
                               bool is_streaming,
                               bool is_static) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, State::STOPPED);
  DCHECK(demuxer);

  // Once the pipeline is started, we want to call the seeked callback but
  // without a time update.
  pending_startup_ = true;
  pending_seeked_cb_ = true;
  state_ = State::STARTING;

  demuxer_ = demuxer;
  is_streaming_ = is_streaming;
  is_static_ = is_static;
  pipeline_->Start(start_type, demuxer, renderer_factory_cb_.Run(), client,
                   base::Bind(&PipelineController::OnPipelineStatus,
                              weak_factory_.GetWeakPtr(),
                              start_type == Pipeline::StartType::kNormal
                                  ? State::PLAYING
                                  : State::PLAYING_OR_SUSPENDED));
}

void PipelineController::Seek(base::TimeDelta time, bool time_updated) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // It would be slightly more clear to set this in Dispatch(), but we want to
  // be sure it gets updated even if the seek is elided.
  if (time_updated)
    pending_time_updated_ = true;
  pending_seeked_cb_ = true;
  pending_seek_except_start_ = true;

  // If we are already seeking to |time|, and the media is static, elide the
  // seek.
  if ((state_ == State::SEEKING || state_ == State::RESUMING) &&
      seek_time_ == time && is_static_) {
    pending_seek_ = false;
    return;
  }

  pending_seek_time_ = time;
  pending_seek_ = true;
  Dispatch();
}

// TODO(sandersd): It may be easier to use this interface if |suspended_cb_| is
// executed when Suspend() is called while already suspended.
void PipelineController::Suspend() {
  DCHECK(thread_checker_.CalledOnValidThread());
  pending_resume_ = false;
  if (state_ != State::SUSPENDING && state_ != State::SUSPENDED) {
    pending_suspend_ = true;
    Dispatch();
  }
}

void PipelineController::Resume() {
  DCHECK(thread_checker_.CalledOnValidThread());
  pending_suspend_ = false;
  if (state_ == State::SUSPENDING || state_ == State::SUSPENDED) {
    pending_resume_ = true;
    Dispatch();
  }
}

bool PipelineController::IsStable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return state_ == State::PLAYING;
}

bool PipelineController::IsPendingSeek() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return pending_seek_except_start_;
}

bool PipelineController::IsSuspended() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return (pending_suspend_ || state_ == State::SUSPENDING ||
          state_ == State::SUSPENDED) &&
         !pending_resume_;
}

bool PipelineController::IsPipelineSuspended() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return state_ == State::SUSPENDED;
}

void PipelineController::OnPipelineStatus(State expected_state,
                                          PipelineStatus pipeline_status) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (pipeline_status != PIPELINE_OK) {
    error_cb_.Run(pipeline_status);
    return;
  }

  State old_state = state_;
  state_ = expected_state;

  // Resolve ambiguity of the current state if we may have suspended in startup.
  if (state_ == State::PLAYING_OR_SUSPENDED) {
    waiting_for_seek_ = false;
    state_ = pipeline_->IsSuspended() ? State::SUSPENDED : State::PLAYING;
  }

  if (state_ == State::PLAYING) {
    // Start(), Seek(), or Resume() completed; we can be sure that
    // |demuxer_| got the seek it was waiting for.
    waiting_for_seek_ = false;

    // TODO(avayvod): Remove resumed callback after https://crbug.com/678374 is
    // properly fixed.
    if (old_state == State::RESUMING) {
      DCHECK(!pipeline_->IsSuspended());
      resumed_cb_.Run();
    }
  }

  if (state_ == State::SUSPENDED) {
    DCHECK(pipeline_->IsSuspended());

    // Warning: possibly reentrant. The state may change inside this callback.
    // It must be safe to call Dispatch() twice in a row here.
    suspended_cb_.Run();
  }

  Dispatch();
}

// Note: Dispatch() may be called re-entrantly (by callbacks internally) or
// twice in a row (by OnPipelineStatus()).
void PipelineController::Dispatch() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Suspend/resume transitions take priority because seeks before a suspend
  // are wasted, and seeks after can be merged into the resume operation.
  if (pending_suspend_ && state_ == State::PLAYING) {
    pending_suspend_ = false;
    state_ = State::SUSPENDING;
    pipeline_->Suspend(base::Bind(&PipelineController::OnPipelineStatus,
                                  weak_factory_.GetWeakPtr(),
                                  State::SUSPENDED));
    return;
  }

  if (pending_resume_ && state_ == State::SUSPENDED) {
    // If there is a pending seek, resume to that time instead...
    if (pending_seek_) {
      seek_time_ = pending_seek_time_;
      pending_seek_ = false;
    } else {
      seek_time_ = pipeline_->GetMediaTime();
    }

    // ...unless the media is streaming, in which case we resume at the start
    // because seeking doesn't work well.
    if (is_streaming_ && !seek_time_.is_zero()) {
      seek_time_ = base::TimeDelta();

      // In this case we want to make sure that the controls get updated
      // immediately, so we don't try to hide the seek.
      pending_time_updated_ = true;
    }

    // Tell |demuxer_| to expect our resume.
    DCHECK(!waiting_for_seek_);
    waiting_for_seek_ = true;
    demuxer_->StartWaitingForSeek(seek_time_);

    pending_resume_ = false;
    state_ = State::RESUMING;
    before_resume_cb_.Run();
    pipeline_->Resume(renderer_factory_cb_.Run(), seek_time_,
                      base::Bind(&PipelineController::OnPipelineStatus,
                                 weak_factory_.GetWeakPtr(), State::PLAYING));
    return;
  }

  // If we have pending operations, and a seek is ongoing, abort it.
  if ((pending_seek_ || pending_suspend_ || pending_audio_track_change_ ||
       pending_video_track_change_) &&
      waiting_for_seek_) {
    // If there is no pending seek, return the current seek to pending status.
    if (!pending_seek_) {
      pending_seek_time_ = seek_time_;
      pending_seek_ = true;
    }

    // CancelPendingSeek() may be reentrant, so update state first and return
    // immediately.
    waiting_for_seek_ = false;
    demuxer_->CancelPendingSeek(pending_seek_time_);
    return;
  }

  // We can only switch tracks if we are not in a transitioning state already.
  if ((pending_audio_track_change_ || pending_video_track_change_) &&
      (state_ == State::PLAYING || state_ == State::SUSPENDED)) {
    State old_state = state_;
    state_ = State::SWITCHING_TRACKS;

    // Attempt to do a track change _before_ attempting a seek operation,
    // otherwise the seek will apply to the old tracks instead of the new
    // one(s). Also attempt audio before video.
    if (pending_audio_track_change_) {
      pending_audio_track_change_ = false;
      pipeline_->OnEnabledAudioTracksChanged(
          pending_audio_track_change_ids_,
          base::BindOnce(&PipelineController::OnTrackChangeComplete,
                         weak_factory_.GetWeakPtr(), old_state));
      return;
    }

    if (pending_video_track_change_) {
      pending_video_track_change_ = false;
      pipeline_->OnSelectedVideoTrackChanged(
          pending_video_track_change_id_,
          base::BindOnce(&PipelineController::OnTrackChangeComplete,
                         weak_factory_.GetWeakPtr(), old_state));
      return;
    }
  }

  // Ordinary seeking.
  if (pending_seek_ && state_ == State::PLAYING) {
    seek_time_ = pending_seek_time_;

    // Tell |demuxer_| to expect our seek.
    DCHECK(!waiting_for_seek_);
    waiting_for_seek_ = true;
    demuxer_->StartWaitingForSeek(seek_time_);

    pending_seek_ = false;
    state_ = State::SEEKING;
    pipeline_->Seek(seek_time_,
                    base::Bind(&PipelineController::OnPipelineStatus,
                               weak_factory_.GetWeakPtr(), State::PLAYING));
    return;
  }

  // If |state_| is PLAYING and we didn't trigger an operation above then we
  // are in a stable state. If there is a seeked callback pending, emit it.
  //
  // We also need to emit it if we completed suspended startup.
  if (pending_seeked_cb_ &&
      (state_ == State::PLAYING ||
       (state_ == State::SUSPENDED && pending_startup_))) {
    // |seeked_cb_| may be reentrant, so update state first and return
    // immediately.
    pending_startup_ = false;
    pending_seeked_cb_ = false;
    pending_seek_except_start_ = false;
    bool was_pending_time_updated = pending_time_updated_;
    pending_time_updated_ = false;
    seeked_cb_.Run(was_pending_time_updated);
    return;
  }
}

void PipelineController::Stop() {
  if (state_ == State::STOPPED)
    return;

  demuxer_ = nullptr;
  waiting_for_seek_ = false;
  pending_seeked_cb_ = false;
  pending_seek_except_start_ = false;
  pending_time_updated_ = false;
  pending_seek_ = false;
  pending_suspend_ = false;
  pending_resume_ = false;
  pending_audio_track_change_ = false;
  pending_video_track_change_ = false;
  state_ = State::STOPPED;

  pipeline_->Stop();
}

bool PipelineController::IsPipelineRunning() const {
  return pipeline_->IsRunning();
}

double PipelineController::GetPlaybackRate() const {
  return pipeline_->GetPlaybackRate();
}

void PipelineController::SetPlaybackRate(double playback_rate) {
  pipeline_->SetPlaybackRate(playback_rate);
}

float PipelineController::GetVolume() const {
  return pipeline_->GetVolume();
}

void PipelineController::SetVolume(float volume) {
  pipeline_->SetVolume(volume);
}

base::TimeDelta PipelineController::GetMediaTime() const {
  return pipeline_->GetMediaTime();
}

Ranges<base::TimeDelta> PipelineController::GetBufferedTimeRanges() const {
  return pipeline_->GetBufferedTimeRanges();
}

base::TimeDelta PipelineController::GetMediaDuration() const {
  return pipeline_->GetMediaDuration();
}

bool PipelineController::DidLoadingProgress() {
  return pipeline_->DidLoadingProgress();
}

PipelineStatistics PipelineController::GetStatistics() const {
  return pipeline_->GetStatistics();
}

void PipelineController::SetCdm(CdmContext* cdm_context,
                                const CdmAttachedCB& cdm_attached_cb) {
  pipeline_->SetCdm(cdm_context, cdm_attached_cb);
}

void PipelineController::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& enabled_track_ids) {
  DCHECK(thread_checker_.CalledOnValidThread());

  pending_audio_track_change_ = true;
  pending_audio_track_change_ids_ = enabled_track_ids;

  Dispatch();
}

void PipelineController::OnSelectedVideoTrackChanged(
    base::Optional<MediaTrack::Id> selected_track_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  pending_video_track_change_ = true;
  pending_video_track_change_id_ = selected_track_id;

  Dispatch();
}

void PipelineController::FireOnTrackChangeCompleteForTesting(State set_to) {
  OnTrackChangeComplete(set_to);
}

void PipelineController::OnTrackChangeComplete(State previous_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == State::SWITCHING_TRACKS)
    state_ = previous_state;

  // Other track changed or seek/suspend/resume, etc may be waiting.
  Dispatch();
}

}  // namespace media
