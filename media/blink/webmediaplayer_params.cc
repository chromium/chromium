// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_params.h"

#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "media/base/audio_renderer_sink.h"

namespace media {

WebMediaPlayerParams::WebMediaPlayerParams(
    std::unique_ptr<MediaLog> media_log,
    const DeferLoadCB& defer_load_cb,
    const scoped_refptr<SwitchableAudioRendererSink>& audio_renderer_sink,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>&
        video_frame_compositor_task_runner,
    const AdjustAllocatedMemoryCB& adjust_allocated_memory_cb,
    blink::WebContentDecryptionModule* initial_cdm,
    RequestRoutingTokenCallback request_routing_token_cb,
    base::WeakPtr<MediaObserver> media_observer,
    bool enable_instant_source_buffer_gc,
    bool embedded_media_experience_enabled,
    mojo::PendingRemote<mojom::MediaMetricsProvider> metrics_provider,
    CreateSurfaceLayerBridgeCB create_bridge_callback,
    scoped_refptr<viz::ContextProvider> context_provider,
    blink::WebMediaPlayer::SurfaceLayerMode use_surface_layer_for_video,
    bool is_background_suspend_enabled,
    bool is_background_video_playback_enabled,
    bool is_background_video_track_optimization_supported)
    : defer_load_cb_(defer_load_cb),
      audio_renderer_sink_(audio_renderer_sink),
      media_log_(std::move(media_log)),
      media_task_runner_(media_task_runner),
      worker_task_runner_(worker_task_runner),
      compositor_task_runner_(compositor_task_runner),
      video_frame_compositor_task_runner_(video_frame_compositor_task_runner),
      adjust_allocated_memory_cb_(adjust_allocated_memory_cb),
      initial_cdm_(initial_cdm),
      request_routing_token_cb_(std::move(request_routing_token_cb)),
      media_observer_(media_observer),
      enable_instant_source_buffer_gc_(enable_instant_source_buffer_gc),
      embedded_media_experience_enabled_(embedded_media_experience_enabled),
      metrics_provider_(std::move(metrics_provider)),
      create_bridge_callback_(std::move(create_bridge_callback)),
      context_provider_(std::move(context_provider)),
      use_surface_layer_for_video_(use_surface_layer_for_video),
      is_background_suspend_enabled_(is_background_suspend_enabled),
      is_background_video_playback_enabled_(
          is_background_video_playback_enabled),
      is_background_video_track_optimization_supported_(
          is_background_video_track_optimization_supported) {}

WebMediaPlayerParams::~WebMediaPlayerParams() = default;

}  // namespace media
