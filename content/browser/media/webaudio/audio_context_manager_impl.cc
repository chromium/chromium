// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/webaudio/audio_context_manager_impl.h"

#include <utility>

#include "base/time/default_tick_clock.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace content {

namespace {

// Returns the time in milleseconds following these rules:
//  - if the time is below 10 seconds, return the raw value;
//  - otherwise, return the value rounded to the closes second.
int64_t GetBucketedTimeInMilliseconds(const base::TimeDelta& time) {
  if (time.InMilliseconds() < 10 * base::Time::kMillisecondsPerSecond)
    return time.InMilliseconds();
  return time.InSeconds() * base::Time::kMillisecondsPerSecond;
}

}  // namespace

void AudioContextManagerImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver) {
  CHECK(render_frame_host);

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new AudioContextManagerImpl(*render_frame_host, std::move(receiver));
}

AudioContextManagerImpl& AudioContextManagerImpl::CreateForTesting(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver) {
  return *new AudioContextManagerImpl(render_frame_host, std::move(receiver));
}

AudioContextManagerImpl::AudioContextManagerImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver)
    : DocumentService(render_frame_host, std::move(receiver)),
      clock_(base::DefaultTickClock::GetInstance()) {}

AudioContextManagerImpl::~AudioContextManagerImpl() {
  // Takes care pending "audible start" times.
  base::TimeTicks now = clock_->NowTicks();
  for (const auto& entry : pending_audible_durations_) {
    if (!entry.second.is_null())
      RecordAudibleTime(now - entry.second);
  }
  pending_audible_durations_.clear();
}

void AudioContextManagerImpl::AudioContextAudiblePlaybackStarted(
    int32_t audio_context_id) {
  DCHECK(pending_audible_durations_[audio_context_id].is_null());

  // Keeps track of the start audible time for this context.
  pending_audible_durations_[audio_context_id] = clock_->NowTicks();

  static_cast<RenderFrameHostImpl&>(render_frame_host())
      .AudioContextPlaybackStarted(audio_context_id);
}

void AudioContextManagerImpl::AudioContextAudiblePlaybackStopped(
    int32_t audio_context_id) {
  base::TimeTicks then = pending_audible_durations_[audio_context_id];
  DCHECK(!then.is_null());

  RecordAudibleTime(clock_->NowTicks() - then);

  // Resets the context slot because the context is not audible.
  pending_audible_durations_[audio_context_id] = base::TimeTicks();

  static_cast<RenderFrameHostImpl&>(render_frame_host())
      .AudioContextPlaybackStopped(audio_context_id);
}

void AudioContextManagerImpl::RecordAudibleTime(base::TimeDelta audible_time) {
  DCHECK(!audible_time.is_zero());

  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  DCHECK(ukm_recorder);

  // AudioContextManagerImpl is created when the AudioContext starts running.
  // As the AudioContext is suspended during prerendering even if the autoplay
  // is permitted, it is ensured that the lifecycle state could not be
  // kPrerendering here. This assumption is needed to record UKMs below.
  CHECK(!render_frame_host().IsInLifecycleState(
      RenderFrameHost::LifecycleState::kPrerendering));

  ukm::builders::Media_WebAudio_AudioContext_AudibleTime(
      render_frame_host().GetPageUkmSourceId())
      .SetIsMainFrame(render_frame_host().IsInPrimaryMainFrame())
      .SetAudibleTime(GetBucketedTimeInMilliseconds(audible_time))
      .Record(ukm_recorder);
}

}  // namespace content
