// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/webaudio/audio_context_manager_impl.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
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
  // Takes care of pending "audible start" times.
  base::TimeTicks now = clock_->NowTicks();
  for (const auto& entry : pending_audible_durations_) {
    base::TimeDelta audible_time = now - entry.second;
    if (audible_time.is_positive()) {
      RecordAudibleTime(audible_time);
    }
  }
  UMA_HISTOGRAM_EXACT_LINEAR("WebAudio.AudioContext.ConcurrentAudioContexts",
                             max_concurrent_audio_contexts_,
                             /*exclusive_max=*/101);
}

void AudioContextManagerImpl::AudioContextAudiblePlaybackStarted(
    uint32_t audio_context_id) {
  auto [it, inserted] = pending_audible_durations_.try_emplace(
      audio_context_id, clock_->NowTicks());
  if (!inserted) {
    mojo::ReportBadMessage(
        "AudioContextAudiblePlaybackStarted() called more than once with the "
        "same audio_context_id");
    return;
  }

  static_cast<RenderFrameHostImpl&>(render_frame_host())
      .AudioContextPlaybackStarted(audio_context_id);
}

void AudioContextManagerImpl::AudioContextAudiblePlaybackStopped(
    uint32_t audio_context_id) {
  auto it = pending_audible_durations_.find(audio_context_id);

  // The browser process should not trust the renderer. If the renderer calls
  // Stopped without a matching Started call (or if the ID is unknown), it is
  // a protocol error.
  if (it == pending_audible_durations_.end()) {
    mojo::ReportBadMessage(
        "AudioContextAudiblePlaybackStopped() called without a matching "
        "AudioContextAudiblePlaybackStarted()");
    return;
  }

  base::TimeTicks then = it->second;
  base::TimeDelta duration = clock_->NowTicks() - then;

  // It is possible for the duration to be zero if Started and Stopped are
  // called in extremely rapid succession (within the same clock tick).
  if (duration.is_positive()) {
    RecordAudibleTime(duration);
  }

  // Resets the context slot because the context is not audible.
  pending_audible_durations_.erase(it);

  static_cast<RenderFrameHostImpl&>(render_frame_host())
      .AudioContextPlaybackStopped(audio_context_id);
}

void AudioContextManagerImpl::RecordAudibleTime(base::TimeDelta audible_time) {
  DCHECK(audible_time.is_positive());

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

void AudioContextManagerImpl::AudioContextCreated(uint32_t audio_context_id) {
  concurrent_audio_context_ids_.insert(audio_context_id);
  max_concurrent_audio_contexts_ = std::max(
      max_concurrent_audio_contexts_, concurrent_audio_context_ids_.size());
}

void AudioContextManagerImpl::AudioContextClosed(uint32_t audio_context_id) {
  concurrent_audio_context_ids_.erase(audio_context_id);
}

}  // namespace content
