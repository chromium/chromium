// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_BITSTREAM_CONVERTER_H_
#define MEDIA_FILTERS_FFMPEG_BITSTREAM_CONVERTER_H_

#include "media/base/media_export.h"

struct AVPacket;

namespace media {

// Interface for classes that allow reformatting of FFmpeg bitstreams
class MEDIA_EXPORT FFmpegBitstreamConverter {
 public:
  virtual ~FFmpegBitstreamConverter() {}

  // Reads the data in packet, and then overwrites this data with the
  // converted version of packet
  //
  // Returns false if conversion failed. In this case, |packet| should not be
  // changed.
  virtual bool ConvertPacket(AVPacket* packet) = 0;
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_BITSTREAM_CONVERTER_H_
