// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_RENDERER_H_
#define MEDIA_BASE_RENDERER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/buffering_state.h"
#include "media/base/cdm_context.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"

namespace media {

class MediaResource;
class RendererClient;

class MEDIA_EXPORT Renderer {
 public:
  Renderer();

  // Stops rendering and fires any pending callbacks.
  virtual ~Renderer();

  // Initializes the Renderer with |media_resource|, executing |init_cb| upon
  // completion. |media_resource| must be valid for the lifetime of the Renderer
  // object.  |init_cb| must only be run after this method has returned. Firing
  // |init_cb| may result in the immediate destruction of the caller, so it must
  // be run only prior to returning.
  virtual void Initialize(MediaResource* media_resource,
                          RendererClient* client,
                          const PipelineStatusCB& init_cb) = 0;

  // Associates the |cdm_context| with this Renderer for decryption (and
  // decoding) of media data, then fires |cdm_attached_cb| with the result.
  virtual void SetCdm(CdmContext* cdm_context,
                      const CdmAttachedCB& cdm_attached_cb) = 0;

  // The following functions must be called after Initialize().

  // Discards any buffered data, executing |flush_cb| when completed.
  virtual void Flush(const base::Closure& flush_cb) = 0;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(Renderer);
};

}  // namespace media

#endif  // MEDIA_BASE_RENDERER_H_
