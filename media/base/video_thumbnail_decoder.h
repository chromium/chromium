// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_THUMBNAIL_DECODER_H_
#define MEDIA_BASE_VIDEO_THUMBNAIL_DECODER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/media_export.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"

namespace media {

class VideoFrame;

// Class to decode a single video frame for video thumbnail generation.
class MEDIA_EXPORT VideoThumbnailDecoder {
 public:
  using VideoFrameCallback =
      base::OnceCallback<void(scoped_refptr<VideoFrame>)>;

  // Creates the thumbnail decoder with |decoder| that performs the actual work.
  // |encoded_data| is the encoded frame extracted with ffmpeg. A video frame
  // will be returned through |video_frame_callback|.
  VideoThumbnailDecoder(std::unique_ptr<VideoDecoder> decoder,
                        const VideoDecoderConfig& config,
                        std::vector<uint8_t> encoded_data);
  VideoThumbnailDecoder(const VideoThumbnailDecoder&) = delete;
  VideoThumbnailDecoder& operator=(const VideoThumbnailDecoder&) = delete;
  ~VideoThumbnailDecoder();

  // Starts to decode the video frame.
  void Start(VideoFrameCallback video_frame_callback);

 private:
  void OnVideoDecoderInitialized(DecoderStatus status);
  void OnVideoBufferDecoded(DecoderStatus status);
  void OnEosBufferDecoded(DecoderStatus status);

  // Called when the output frame is generated.
  void OnVideoFrameDecoded(scoped_refptr<VideoFrame> frame);

  void NotifyComplete(scoped_refptr<VideoFrame> frame);

  // The decoder that does the actual decoding.
  std::unique_ptr<VideoDecoder> decoder_;
  const VideoDecoderConfig config_;

  // The demuxed, encoded video frame waiting to be decoded.
  std::vector<uint8_t> encoded_data_;

  VideoFrameCallback video_frame_callback_;
  base::WeakPtrFactory<VideoThumbnailDecoder> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_THUMBNAIL_DECODER_H_
