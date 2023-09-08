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
#include "media/filters/hls_rendition.h"

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

  // The HlsRenditionHost owns the only implementation of HlsCodecDetector
  // and uses it only on a single thread, where it is also deleted. Keeping
  // a raw pointer to that HlsRenditionHost is therefore safe.
  HlsCodecDetector(MediaLog* media_log, HlsRenditionHost* rendition_host_);

  void DetermineContainerAndCodec(std::unique_ptr<HlsDataSourceStream> stream,
                                  CodecCallback cb);
  void DetermineContainerOnly(std::unique_ptr<HlsDataSourceStream> stream,
                              CodecCallback cb);

 private:
  void OnStreamFetched(bool container_only,
                       HlsDataSourceStreamManager::ReadResult stream);
  void DetermineContainer(bool container_only,
                          const uint8_t* data,
                          size_t size);
  void ParserInit(const StreamParser::InitParameters& params);
  bool OnNewConfigMP2T(std::unique_ptr<MediaTracks> tracks);
  bool OnNewBuffers(const StreamParser::BufferQueueMap& buffers);
  void OnEncryptedMediaInit(EmeInitDataType type,
                            const std::vector<uint8_t>& data);
  void AddCodecToResponse(std::string codec);

  std::unique_ptr<MediaLog> log_;
  CodecCallback callback_;
  std::unique_ptr<media::StreamParser> parser_;
  std::string codec_response_;
  std::string container_;
  raw_ptr<HlsRenditionHost> rendition_host_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<HlsCodecDetector> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_CODEC_DETECTOR_H_
