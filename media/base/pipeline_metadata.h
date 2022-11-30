// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PIPELINE_METADATA_H_
#define MEDIA_BASE_PIPELINE_METADATA_H_

#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_transformation.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Metadata describing a pipeline once it has been initialized.
struct MEDIA_EXPORT PipelineMetadata {
  PipelineMetadata();
  ~PipelineMetadata();

  // Required by Chromium style: Complex class/struct needs an explicit
  // out-of-line copy constructor.
  PipelineMetadata(const PipelineMetadata&);

  // On Android, when using the MediaPlayerRenderer, |has_video| and |has_audio|
  // will be true, but the respective configs will be empty.
  // Do not make any assumptions on the validity of configs based off of the
  // presence of audio/video.
  bool has_audio;
  bool has_video;
  AudioDecoderConfig audio_decoder_config;
  VideoDecoderConfig video_decoder_config;
  gfx::Size natural_size;  // Rotated according to
                           // |video_decoder_config|.video_rotation().
  base::Time timeline_offset;
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_METADATA_H_
