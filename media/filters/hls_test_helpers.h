// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_TEST_HELPERS_H_
#define MEDIA_FILTERS_HLS_TEST_HELPERS_H_

#include "media/filters/hls_codec_detector.h"
#include "media/filters/hls_data_source_provider.h"
#include "media/filters/hls_rendition.h"
#include "media/filters/manifest_demuxer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockCodecDetector : public HlsCodecDetector {
 public:
  ~MockCodecDetector() override;
  MockCodecDetector();
  MOCK_METHOD(void,
              DetermineContainerAndCodec,
              (std::unique_ptr<HlsDataSourceStream>, CodecCallback),
              (override));
  MOCK_METHOD(void,
              DetermineContainerOnly,
              (std::unique_ptr<HlsDataSourceStream> stream, CodecCallback cb),
              (override));
};

class MockHlsDataSourceProvider : public HlsDataSourceProvider {
 public:
  MockHlsDataSourceProvider();
  ~MockHlsDataSourceProvider() override;
  MOCK_METHOD(void,
              ReadFromUrl,
              (GURL,
               absl::optional<hls::types::ByteRange>,
               HlsDataSourceProvider::ReadCb),
              (override));
  MOCK_METHOD(void,
              ReadFromExistingStream,
              (std::unique_ptr<HlsDataSourceStream>,
               HlsDataSourceProvider::ReadCb),
              (override));
  MOCK_METHOD(void,
              AbortPendingReads,
              (base::OnceClosure callback),
              (override));
};

class StringHlsDataSourceStreamFactory {
 public:
  static std::unique_ptr<HlsDataSourceStream> CreateStream(std::string content);
};

class FileHlsDataSourceStreamFactory {
 public:
  static std::unique_ptr<HlsDataSourceStream> CreateStream(std::string file);
};

class MockManifestDemuxerEngineHost : public ManifestDemuxerEngineHost {
 public:
  MockManifestDemuxerEngineHost();
  ~MockManifestDemuxerEngineHost() override;
  MOCK_METHOD(bool,
              AddRole,
              (base::StringPiece, std::string, std::string),
              (override));
  MOCK_METHOD(void, RemoveRole, (base::StringPiece), (override));
  MOCK_METHOD(void, SetSequenceMode, (base::StringPiece, bool), (override));
  MOCK_METHOD(void, SetDuration, (double), (override));
  MOCK_METHOD(Ranges<base::TimeDelta>,
              GetBufferedRanges,
              (base::StringPiece),
              (override));
  MOCK_METHOD(void,
              Remove,
              (base::StringPiece, base::TimeDelta, base::TimeDelta),
              (override));
  MOCK_METHOD(
      void,
      RemoveAndReset,
      (base::StringPiece, base::TimeDelta, base::TimeDelta, base::TimeDelta*),
      (override));
  MOCK_METHOD(void,
              SetGroupStartIfParsingAndSequenceMode,
              (base::StringPiece, base::TimeDelta),
              (override));
  MOCK_METHOD(void,
              EvictCodedFrames,
              (base::StringPiece, base::TimeDelta, size_t),
              (override));
  MOCK_METHOD(bool,
              AppendAndParseData,
              (base::StringPiece,
               base::TimeDelta,
               base::TimeDelta,
               base::TimeDelta*,
               const uint8_t*,
               size_t),
              (override));
  MOCK_METHOD(void, OnError, (PipelineStatus), (override));
  MOCK_METHOD(void, RequestSeek, (base::TimeDelta), (override));
  MOCK_METHOD(void,
              SetGroupStartTimestamp,
              (base::StringPiece role, base::TimeDelta time),
              (override));
  MOCK_METHOD(void, SetEndOfStream, (), (override));
  MOCK_METHOD(void, UnsetEndOfStream, (), (override));
};

class MockHlsRenditionHost : public HlsRenditionHost {
 public:
  MockHlsRenditionHost();
  ~MockHlsRenditionHost() override;
  MOCK_METHOD(void,
              ReadFromUrl,
              (GURL uri,
               bool read_chunked,
               absl::optional<hls::types::ByteRange> range,
               HlsDataSourceProvider::ReadCb cb),
              (override));

  MOCK_METHOD(void,
              UpdateRenditionManifestUri,
              (std::string, GURL, base::OnceClosure),
              (override));

  MOCK_METHOD(void,
              ReadStream,
              (std::unique_ptr<HlsDataSourceStream>,
               HlsDataSourceProvider::ReadCb),
              (override));

  MOCK_METHOD(void, UpdateNetworkSpeed, (uint64_t), (override));
};

class MockHlsRendition : public HlsRendition {
 public:
  MockHlsRendition();
  ~MockHlsRendition() override;

  MOCK_METHOD(void,
              CheckState,
              (base::TimeDelta time,
               double rate,
               ManifestDemuxer::DelayCallback cb),
              (override));
  MOCK_METHOD(ManifestDemuxer::SeekResponse,
              Seek,
              (base::TimeDelta time),
              (override));
  MOCK_METHOD(void, StartWaitingForSeek, (), (override));
  MOCK_METHOD(absl::optional<base::TimeDelta>, GetDuration, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void,
              UpdatePlaylist,
              (scoped_refptr<hls::MediaPlaylist>, std::optional<GURL>),
              (override));
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_TEST_HELPERS_H_
