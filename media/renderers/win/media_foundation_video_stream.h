// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_VIDEO_STREAM_H_
#define MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_VIDEO_STREAM_H_

#include <mfapi.h>
#include <mfidl.h>

#include "media/renderers/win/media_foundation_stream_wrapper.h"

#include "media/media_buildflags.h"

namespace media {

// The common video stream.
class MediaFoundationVideoStream : public MediaFoundationStreamWrapper {
 public:
  static HRESULT Create(int stream_id,
                        IMFMediaSource* parent_source,
                        DemuxerStream* demuxer_stream,
                        std::unique_ptr<MediaLog> media_log,
                        MediaFoundationStreamWrapper** stream_out);

  bool IsEncrypted() const override;

 protected:
  HRESULT GetMediaType(IMFMediaType** media_type_out) override;
};

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// The H264 specific video stream.
class MediaFoundationH264VideoStream : public MediaFoundationVideoStream {
 protected:
  HRESULT GetMediaType(IMFMediaType** media_type_out) override;
  bool AreFormatChangesEnabled() override;
};
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

#if BUILDFLAG(ENABLE_PLATFORM_HEVC) || BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
// The HEVC specific video stream.
class MediaFoundationHEVCVideoStream : public MediaFoundationVideoStream {
 protected:
  HRESULT GetMediaType(IMFMediaType** media_type_out) override;
  bool AreFormatChangesEnabled() override;
};
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) ||
        // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)

}  // namespace media

#endif  // MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_VIDEO_STREAM_H_
