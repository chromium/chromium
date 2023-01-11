// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/silent_sink_suspender.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_glitch_info.h"

namespace media {

SilentSinkSuspender::SilentSinkSuspender(
    AudioRendererSink::RenderCallback* callback,
    base::TimeDelta silence_timeout,
    const AudioParameters& params,
    scoped_refptr<AudioRendererSink> sink,
    scoped_refptr<base::SingleThreadTaskRunner> worker)
    : callback_(callback),
      params_(params),
      sink_(std::move(sink)),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      silence_timeout_(silence_timeout),
      fake_sink_(std::move(worker), params_),
      sink_transition_callback_(
          base::BindRepeating(&SilentSinkSuspender::TransitionSinks,
                              base::Unretained(this))) {
  DCHECK(params_.IsValid());
  DCHECK(sink_);
  DCHECK(callback_);
  DCHECK(task_runner_->BelongsToCurrentThread());
}

SilentSinkSuspender::~SilentSinkSuspender() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  fake_sink_.Stop();
}

int SilentSinkSuspender::Render(base::TimeDelta delay,
                                base::TimeTicks delay_timestamp,
                                const AudioGlitchInfo& glitch_info,
                                AudioBus* dest) {
  // Lock required since AudioRendererSink::Pause() is not synchronous, we need
  // to discard these calls during the transition to the fake sink.
  base::AutoLock al(transition_lock_);
  if (is_using_fake_sink_ && dest) {
    // Audio should be silent at this point, if not, it will be handled once the
    // transition to the fake sink is complete.
    dest->Zero();
    return dest->frames();
  }

  // When we're using the |fake_sink_| a null destination will be sent; we store
  // the audio data for a future transition out of silence.
  if (!dest) {
    DCHECK(is_using_fake_sink_);
    // |delay_timestamp| contains the value cached at
    // |latest_output_delay_timestamp_|
    // so we simulate the real sink output, promoting |delay_timestamp| with
    // |elapsed_time|.
    DCHECK_EQ(delay_timestamp, latest_output_delay_timestamp_);
    base::TimeDelta elapsed_time =
        base::TimeTicks::Now() - fake_sink_transition_time_;
    delay_timestamp += elapsed_time;

    // If we have no buffers or a transition is pending, one or more extra
    // Render() calls have occurred in before TransitionSinks() can run, so we
    // store this data for the eventual transition.
    if (buffers_after_silence_.empty() || is_transition_pending_)
      buffers_after_silence_.push_back(AudioBus::Create(params_));
    dest = buffers_after_silence_.back().get();
  } else if (!buffers_after_silence_.empty()) {
    // Drain any non-silent transitional buffers before queuing more audio data.
    // Note: These do not skew clocks derived from frame count since we don't
    // issue Render() to the client when returning these buffers.
    DCHECK(!is_using_fake_sink_);
    buffers_after_silence_.front()->CopyTo(dest);
    buffers_after_silence_.pop_front();
    return dest->frames();
  }

  // Pass-through to client and request rendering.
  callback_->Render(delay, delay_timestamp, glitch_info, dest);

  // Check for silence or real audio data and transition if necessary.
  if (!dest->AreFramesZero() || !detect_silence_) {
    first_silence_time_ = base::TimeTicks();
    if (is_using_fake_sink_) {
      is_transition_pending_ = true;
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(sink_transition_callback_.callback(), false));
      return dest->frames();
    }
  } else if (!is_using_fake_sink_) {
    const base::TimeTicks now = base::TimeTicks::Now();
    if (first_silence_time_.is_null())
      first_silence_time_ = now;
    if (now - first_silence_time_ > silence_timeout_) {
      is_transition_pending_ = true;
      latest_output_delay_ = delay;
      latest_output_delay_timestamp_ = delay_timestamp;
      fake_sink_transition_time_ = now;
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(sink_transition_callback_.callback(), true));
    }
  }

  return dest->frames();
}

void SilentSinkSuspender::OnRenderError() {
  callback_->OnRenderError();
}

void SilentSinkSuspender::OnPaused() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // This is a no-op if the sink isn't running, but must be executed without the
  // |transition_lock_| being held to avoid possible deadlock.
  fake_sink_.Stop();

  base::AutoLock al(transition_lock_);

  // Nothing to do if we haven't touched the sink.
  if (!is_using_fake_sink_ && !is_transition_pending_) {
    first_silence_time_ = base::TimeTicks();
    return;
  }

  // If we've moved over to the fake sink, we just need to stop it and cancel
  // any pending transitions.
  is_using_fake_sink_ = false;
  is_transition_pending_ = false;
  first_silence_time_ = base::TimeTicks();
  sink_transition_callback_.Reset(base::BindRepeating(
      &SilentSinkSuspender::TransitionSinks, base::Unretained(this)));
}

void SilentSinkSuspender::SetDetectSilence(bool detect_silence) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  bool should_transition_to_real_sink = false;
  {
    base::AutoLock lock(transition_lock_);
    detect_silence_ = detect_silence;
    should_transition_to_real_sink = !detect_silence_ && is_using_fake_sink_;

    // If we haven't started the transition abort it.
    if (is_transition_pending_) {
      is_transition_pending_ = false;
      sink_transition_callback_.Reset(base::BindRepeating(
          &SilentSinkSuspender::TransitionSinks, base::Unretained(this)));
    }
  }

  if (should_transition_to_real_sink)
    TransitionSinks(/*use_fake_sink=*/false);
}

bool SilentSinkSuspender::IsUsingFakeSinkForTesting() {
  base::AutoLock al(transition_lock_);
  return is_using_fake_sink_;
}

void SilentSinkSuspender::TransitionSinks(bool use_fake_sink) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Ignore duplicate requests which can occur if the transition takes too long
  // and multiple Render() events occur.
  {
    base::AutoLock al(transition_lock_);
    if (use_fake_sink == is_using_fake_sink_)
      return;
  }

  if (use_fake_sink) {
    sink_->Pause();

    // |sink_| may still be executing Render() at this point or even sometime
    // after this point, so we must acquire the lock to make sure we don't have
    // concurrent Render() execution. Once |is_using_fake_sink_| is set to true,
    // calls from |sink_| will be dropped.
    {
      base::AutoLock al(transition_lock_);
      is_transition_pending_ = false;
      is_using_fake_sink_ = true;
    }
    fake_sink_.Start(base::BindRepeating(
        [](SilentSinkSuspender* suspender, base::TimeDelta frozen_delay,
           base::TimeTicks frozen_delay_timestamp, base::TimeTicks ideal_time,
           base::TimeTicks now) {
          // TODO: Seems that the code in Render() might benefit from the two
          // new timestamps being provided by FakeAudioWorker, in that it's call
          // to base::TimeTicks::Now() can be eliminated (use |now| instead),
          // along with its custom delay timestamp calculations.
          suspender->Render(frozen_delay, frozen_delay_timestamp, {}, nullptr);
        },
        this, latest_output_delay_, latest_output_delay_timestamp_));
  } else {
    fake_sink_.Stop();

    // Despite the fake sink having a synchronous Stop(), if this transition
    // occurs too soon after pausing the real sink, we may have pending Render()
    // calls from before the transition to the fake sink. As such, we need to
    // hold the lock here to avoid any races.
    {
      base::AutoLock al(transition_lock_);
      is_transition_pending_ = false;
      is_using_fake_sink_ = false;
    }
    sink_->Play();
  }
}

}  // namespace media
