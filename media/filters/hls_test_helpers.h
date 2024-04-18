// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_TEST_HELPERS_H_
#define MEDIA_FILTERS_HLS_TEST_HELPERS_H_

#include <string_view>

#include "media/filters/hls_data_source_provider.h"
#include "media/filters/hls_rendition.h"
#include "media/filters/manifest_demuxer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockHlsDataSourceProvider : public HlsDataSourceProvider {
 public:
  MockHlsDataSourceProvider();
  ~MockHlsDataSourceProvider() override;
  MOCK_METHOD(void,
              ReadFromCombinedUrlQueue,
              (HlsDataSourceProvider::SegmentQueue,
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
  static std::unique_ptr<HlsDataSourceStream> CreateStream(
      std::string content,
      bool taint_origin = false);
};

class FileHlsDataSourceStreamFactory {
 public:
  static std::unique_ptr<HlsDataSourceStream> CreateStream(
      std::string file,
      bool taint_origin = false);
};

class MockManifestDemuxerEngineHost : public ManifestDemuxerEngineHost {
 public:
  MockManifestDemuxerEngineHost();
  ~MockManifestDemuxerEngineHost() override;
  MOCK_METHOD(bool,
              AddRole,
              (std::string_view, RelaxedParserSupportedType),
              (override));
  MOCK_METHOD(void, RemoveRole, (std::string_view), (override));
  MOCK_METHOD(void, SetSequenceMode, (std::string_view, bool), (override));
  MOCK_METHOD(void, SetDuration, (double), (override));
  MOCK_METHOD(Ranges<base::TimeDelta>,
              GetBufferedRanges,
              (std::string_view),
              (override));
  MOCK_METHOD(void,
              Remove,
              (std::string_view, base::TimeDelta, base::TimeDelta),
              (override));
  MOCK_METHOD(
      void,
      RemoveAndReset,
      (std::string_view, base::TimeDelta, base::TimeDelta, base::TimeDelta*),
      (override));
  MOCK_METHOD(void,
              SetGroupStartIfParsingAndSequenceMode,
              (std::string_view, base::TimeDelta),
              (override));
  MOCK_METHOD(void,
              EvictCodedFrames,
              (std::string_view, base::TimeDelta, size_t),
              (override));
  MOCK_METHOD(bool,
              AppendAndParseData,
              (std::string_view,
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
              (std::string_view role, base::TimeDelta time),
              (override));
  MOCK_METHOD(void, SetEndOfStream, (), (override));
  MOCK_METHOD(void, UnsetEndOfStream, (), (override));
};

class MockHlsRenditionHost : public HlsRenditionHost {
 public:
  MockHlsRenditionHost();
  ~MockHlsRenditionHost() override;
  MOCK_METHOD(void,
              ReadManifest,
              (const GURL&, HlsDataSourceProvider::ReadCb),
              (override));
  MOCK_METHOD(
      void,
      ReadMediaSegment,
      (const hls::MediaSegment&, bool, bool, HlsDataSourceProvider::ReadCb),
      (override));

  MOCK_METHOD(void,
              UpdateRenditionManifestUri,
              (std::string, GURL, base::OnceCallback<void(bool)>),
              (override));

  MOCK_METHOD(void,
              ReadStream,
              (std::unique_ptr<HlsDataSourceStream>,
               HlsDataSourceProvider::ReadCb),
              (override));

  MOCK_METHOD(void, UpdateNetworkSpeed, (uint64_t), (override));

  MOCK_METHOD(void, SetEndOfStream, (bool), (override));
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
  MOCK_METHOD(std::optional<base::TimeDelta>, GetDuration, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void,
              UpdatePlaylist,
              (scoped_refptr<hls::MediaPlaylist>, std::optional<GURL>),
              (override));
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_TEST_HELPERS_H_
