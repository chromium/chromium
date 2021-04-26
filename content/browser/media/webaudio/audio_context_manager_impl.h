// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_WEBAUDIO_AUDIO_CONTEXT_MANAGER_IMPL_H_
#define CONTENT_BROWSER_MEDIA_WEBAUDIO_AUDIO_CONTEXT_MANAGER_IMPL_H_

#include "content/common/content_export.h"
#include "content/public/browser/frame_service_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom.h"

namespace base {
class TickClock;
}

namespace content {

class RenderFrameHost;
class RenderFrameHostImpl;

// Implements the mojo interface between WebAudio and the browser so that
// WebAudio can report when audible sounds from an AudioContext starts and
// stops. A manager instance can be associated with multiple AudioContexts.
//
// We do not expect to see more than 3~4 AudioContexts per render frame, so
// handling multiple contexts would not be a significant bottle neck.
class CONTENT_EXPORT AudioContextManagerImpl final
    : public content::FrameServiceBase<blink::mojom::AudioContextManager> {
 public:
  explicit AudioContextManagerImpl(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver);
  ~AudioContextManagerImpl() override;

  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver);

  // Notify observers that audible audio started/stopped playing from an
  // AudioContext.
  void AudioContextAudiblePlaybackStarted(int32_t audio_context_id) final;
  void AudioContextAudiblePlaybackStopped(int32_t audio_context_id) final;

  void set_clock_for_testing(base::TickClock* clock) { clock_ = clock; }

 private:
  // Send measured audible duration to UKM database.
  void RecordAudibleTime(base::TimeDelta);

  RenderFrameHostImpl* const render_frame_host_impl_;

  // To track pending audible time. Stores ID of AudioContext (int32_t) and
  // the start time of audible period (base::TimeTicks).
  base::flat_map<int32_t, base::TimeTicks> pending_audible_durations_;

  // Clock used to calculate time between start and stop event. Can be override
  // by tests.
  // It is not owned by the implementation.
  const base::TickClock* clock_;

  DISALLOW_COPY_AND_ASSIGN(AudioContextManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_WEBAUDIO_AUDIO_CONTEXT_MANAGER_IMPL_H_
