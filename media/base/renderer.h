// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_RENDERER_H_
#define MEDIA_BASE_RENDERER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "media/base/buffering_state.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"

namespace media {

class CdmContext;
class MediaResource;
class RendererClient;

// Types of media::Renderer.
// WARNING: These values are reported to metrics. Entries should not be
// renumbered and numeric values should not be reused. When adding new entries,
// also update media::mojom::RendererType & tools/metrics/histograms/enums.xml.
enum class RendererType {
  kRendererImpl = 0,     // RendererImplFactory
  kMojo = 1,             // MojoRendererFactory
  kMediaPlayer = 2,      // MediaPlayerRendererClientFactory
  kCourier = 3,          // CourierRendererFactory
  kFlinging = 4,         // FlingingRendererClientFactory
  kCast = 5,             // CastRendererClientFactory
  kMediaFoundation = 6,  // MediaFoundationRendererClientFactory
  // kFuchsia = 7,       // Deprecated
  kRemoting = 8,       // RemotingRendererFactory for remoting::Receiver
  kCastStreaming = 9,  // PlaybackCommandForwardingRendererFactory
  kContentEmbedderDefined = 10,  // Defined by the content embedder
  kTest = 11,                    // Renderer implementations used in tests
  kMaxValue = kTest,
};

// Get the name of the Renderer for `renderer_type`. The returned name could be
// the actual Renderer class name or a descriptive name.
std::string MEDIA_EXPORT GetRendererName(RendererType renderer_type);

class MEDIA_EXPORT Renderer {
 public:
  Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  // Stops rendering and fires any pending callbacks.
  virtual ~Renderer();

  // Initializes the Renderer with |media_resource|, executing |init_cb| upon
  // completion. |media_resource| must be valid for the lifetime of the Renderer
  // object.  |init_cb| must only be run after this method has returned. Firing
  // |init_cb| may result in the immediate destruction of the caller, so it must
  // be run only prior to returning.
  virtual void Initialize(MediaResource* media_resource,
                          RendererClient* client,
                          PipelineStatusCallback init_cb) = 0;

  // Associates the |cdm_context| with this Renderer for decryption (and
  // decoding) of media data, then fires |cdm_attached_cb| with whether the
  // operation succeeded.
  using CdmAttachedCB = base::OnceCallback<void(bool)>;
  virtual void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb);

  // Specifies a latency hint from the site. Renderers should clamp the hint
  // value to reasonable min and max and use the resulting value as a target
  // latency such that the buffering state reaches HAVE_ENOUGH when this amount
  // of decoded data is buffered. A nullopt hint indicates the user is clearing
  // their preference and the renderer should restore its default buffering
  // thresholds.
  virtual void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) = 0;

  // Sets whether pitch adjustment should be applied when the playback rate is
  // different than 1.0.
  virtual void SetPreservesPitch(bool preserves_pitch);

  // Sets a flag indicating whether the audio stream was played with user
  // activation.
  virtual void SetWasPlayedWithUserActivationAndHighMediaEngagement(
      bool was_played_with_user_activation_and_high_media_engagement);

  // The following functions must be called after Initialize().

  // Discards any buffered data, executing |flush_cb| when completed.
  virtual void Flush(base::OnceClosure flush_cb) = 0;

  // Starts rendering from |time|.
  virtual void StartPlayingFrom(base::TimeDelta time) = 0;

  // Updates the current playback rate. The default playback rate should be 0.
  virtual void SetPlaybackRate(double playback_rate) = 0;

  // Sets the output volume. The default volume should be 1.
  virtual void SetVolume(float volume) = 0;

  // Returns the current media time.
  //
  // This method must be safe to call from any thread.
  virtual base::TimeDelta GetMediaTime() = 0;

  // Provides a list of DemuxerStreams correlating to the tracks which should
  // be played. An empty list would mean that any playing track of the same
  // type should be flushed and disabled. Any provided Streams should be played
  // by whatever mechanism the subclass of Renderer choses for managing it's AV
  // playback.
  virtual void OnSelectedVideoTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb);
  virtual void OnEnabledAudioTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb);

  // Signal to the renderer that there has been a client request to access a
  // VideoFrame. This signal may be used by the renderer to ensure it is
  // operating in a mode which produces a VideoFrame usable by the client.
  // E.g., the MediaFoundationRendererClient on Windows has two modes
  // of operation: Frame Server & Direct Composition. Direct Composition mode
  // does not produce a VideoFrame with an accessible 'data' buffer, so clients
  // cannot access the underlying image data. In order for
  // MediaFoundationRendererClient to produce a VideoFrame with 'data'
  // accessible by the client it must switch to operate in Frame Server mode.
  virtual void OnExternalVideoFrameRequest();

  // Returns the type of the Renderer implementation. Marked as pure virtual to
  // enforce RendererType registration for all Renderer implementations.
  // Note: New implementation should update RendererType.
  virtual RendererType GetRendererType() = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_RENDERER_H_
