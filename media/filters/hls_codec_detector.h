// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_CODEC_DETECTOR_H_
#define MEDIA_FILTERS_HLS_CODEC_DETECTOR_H_

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "media/base/media_log.h"
#include "media/base/media_tracks.h"
#include "media/base/stream_parser.h"
#include "media/filters/hls_data_source_provider.h"
#include "media/filters/hls_demuxer_status.h"

namespace media {

// Helper class for parsing a hls segment to determine a container and codec
// list, which can be used to initialize the chunk demuxer.
class MEDIA_EXPORT HlsCodecDetector {
 public:
  struct ContainerAndCodecs {
    std::string container;
    std::string codecs;
  };

  using CodecCallback = HlsDemuxerStatusCb<ContainerAndCodecs>;

  ~HlsCodecDetector();
  explicit HlsCodecDetector(MediaLog* media_log);

  void DetermineContainerAndCodec(HlsDataSourceStream stream, CodecCallback cb);
  void DetermineContainerOnly(HlsDataSourceStream stream, CodecCallback cb);

 private:
  void ReadStream(bool container_only, HlsDataSourceStream::ReadResult stream);
  void DetermineContainer(bool container_only,
                          const uint8_t* data,
                          size_t size);
  void ParserInit(const StreamParser::InitParameters& params);
  bool OnNewConfigMP2T(std::unique_ptr<MediaTracks> tracks,
                       const StreamParser::TextTrackConfigMap& map);
  bool OnNewBuffers(const StreamParser::BufferQueueMap& buffers);
  void OnEncryptedMediaInit(EmeInitDataType type,
                            const std::vector<uint8_t>& data);
  void AddCodecToResponse(std::string codec);

  std::unique_ptr<MediaLog> log_;
  CodecCallback callback_;
  std::unique_ptr<media::StreamParser> parser_;
  std::string codec_response_;
  std::string container_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<HlsCodecDetector> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_CODEC_DETECTOR_H_
