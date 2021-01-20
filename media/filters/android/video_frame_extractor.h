// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_ANDROID_VIDEO_FRAME_EXTRACTOR_H_
#define MEDIA_FILTERS_ANDROID_VIDEO_FRAME_EXTRACTOR_H_

#include <stdint.h>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/filters/ffmpeg_demuxer.h"

struct AVPacket;
struct AVStream;

namespace media {

class BlockingUrlProtocol;
class DataSource;
class FFmpegBitstreamConverter;
class FFmpegGlue;
class VideoDecoderConfig;

// This class synchronously extracts a video key frame. Should be used in
// sandboxed process since the media data is user input.
// Work flow:
// 1. Demuxes one video frame with FFMPEG, into an AVPacket.
// 2. Adds the bitstream filter, so decoder can understand the input data.
// 3. Returns the encoded video key frame and the decoding configuration for
// later decoding. On Android, currently decoding needs to happen in privileged
// process to access low level graphic card driver on disk.
class MEDIA_EXPORT VideoFrameExtractor {
 public:
  using VideoFrameCallback =
      base::OnceCallback<void(bool success,
                              std::vector<uint8_t> data,
                              const VideoDecoderConfig& decoder_config)>;

  explicit VideoFrameExtractor(DataSource* data_source);
  ~VideoFrameExtractor();

  // Starts to retrieve thumbnail from video frame.
  void Start(VideoFrameCallback video_frame_callback);

 private:
  // Reads one video frame from video stream.
  ScopedAVPacket ReadVideoFrame();

  // Converts the video frame to something that the decoder can understand.
  void ConvertPacket(AVPacket* packet);

  // Called when video frame is successfully extracted.
  void NotifyComplete(std::vector<uint8_t> encoded_frame,
                      const VideoDecoderConfig& config);

  // Called when error happens.
  void OnError();

  // Objects to read video data.
  DataSource* data_source_;
  std::unique_ptr<BlockingUrlProtocol> protocol_;
  std::unique_ptr<FFmpegGlue> glue_;

  // FFMPEG related objects to prepare video frame to decode.
  ScopedAVPacket packet_;
  int video_stream_index_;
  AVStream* video_stream_ = nullptr;
  VideoDecoderConfig video_config_;
  std::unique_ptr<FFmpegBitstreamConverter> bitstream_converter_;

  VideoFrameCallback video_frame_callback_;

  base::WeakPtrFactory<VideoFrameExtractor> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoFrameExtractor);
};

}  // namespace media

#endif  // MEDIA_FILTERS_ANDROID_VIDEO_FRAME_EXTRACTOR_H_
