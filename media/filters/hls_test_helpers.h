// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_TEST_HELPERS_H_
#define MEDIA_FILTERS_HLS_TEST_HELPERS_H_

#include <string_view>

#include "media/base/cross_origin_data_source.h"
#include "media/filters/hls_data_source_provider.h"
#include "media/filters/hls_data_source_provider_impl.h"
#include "media/filters/hls_rendition.h"
#include "media/filters/manifest_demuxer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockDataSource : public CrossOriginDataSource {
 public:
  ~MockDataSource() override;
  MockDataSource();
  // Mocked methods from CrossOriginDataSource
  MOCK_METHOD(bool, IsCorsCrossOrigin, (), (const, override));
  MOCK_METHOD(bool, HasAccessControl, (), (const, override));
  MOCK_METHOD(const std::string&, GetMimeType, (), (const, override));
  MOCK_METHOD(void,
              Initialize,
              (base::OnceCallback<void(bool)> init_cb),
              (override));

  // Mocked methods from DataSource
  MOCK_METHOD(
      void,
      Read,
      (int64_t position, int size, uint8_t* data, DataSource::ReadCB read_cb),
      (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, Abort, (), (override));
  MOCK_METHOD(bool, GetSize, (int64_t * size_out), (override));
  MOCK_METHOD(bool, IsStreaming, (), (override));
  MOCK_METHOD(void, SetBitrate, (int bitrate), (override));
  MOCK_METHOD(bool, PassedTimingAllowOriginCheck, (), (override));
  MOCK_METHOD(bool, WouldTaintOrigin, (), (override));
  MOCK_METHOD(bool, AssumeFullyBuffered, (), (const, override));
  MOCK_METHOD(int64_t, GetMemoryUsage, (), (override));
  MOCK_METHOD(void, SetPreload, (DataSource::Preload preload), (override));
  MOCK_METHOD(GURL, GetUrlAfterRedirects, (), (const, override));
  MOCK_METHOD(void,
              OnBufferingHaveEnough,
              (bool must_cancel_netops),
              (override));
  MOCK_METHOD(void,
              OnMediaPlaybackRateChanged,
              (double playback_rate),
              (override));
  MOCK_METHOD(void, OnMediaIsPlaying, (), (override));
  CrossOriginDataSource* GetAsCrossOriginDataSource() override { return this; }
};

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
               base::TimeDelta*,
               base::span<const uint8_t> data),
              (override));
  MOCK_METHOD(void,
              ResetParserState,
              (std::string_view, base::TimeDelta, base::TimeDelta*),
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
              ReadKey,
              (const hls::MediaSegment::EncryptionData&,
               HlsDataSourceProvider::ReadCb));
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

  MOCK_METHOD(void, AbortPendingReads, (base::OnceClosure), (override));
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

class MockDataSourceFactory
    : public HlsDataSourceProviderImpl::DataSourceFactory {
 public:
  ~MockDataSourceFactory() override;
  MockDataSourceFactory();
  void CreateDataSource(GURL uri, bool ignore_cache, DataSourceCb cb) override;
  void AddReadExpectation(size_t from, size_t to, int response);
  testing::NiceMock<MockDataSource>* PregenerateNextMock();

 private:
  std::unique_ptr<testing::NiceMock<MockDataSource>> next_mock_;
  std::vector<std::tuple<size_t, size_t, int>> read_expectations_;
};

class MockHlsNetworkAccess : public HlsNetworkAccess {
 public:
  ~MockHlsNetworkAccess() override;
  MockHlsNetworkAccess();
  MOCK_METHOD(void,
              ReadKey,
              (const hls::MediaSegment::EncryptionData&,
               HlsDataSourceProvider::ReadCb));
  MOCK_METHOD(void,
              ReadManifest,
              (const GURL& uri, HlsDataSourceProvider::ReadCb cb));
  MOCK_METHOD(void,
              ReadMediaSegment,
              (const hls::MediaSegment&,
               bool read_chunked,
               bool include_init_segment,
               HlsDataSourceProvider::ReadCb cb));
  MOCK_METHOD(void,
              ReadStream,
              (std::unique_ptr<HlsDataSourceStream> stream,
               HlsDataSourceProvider::ReadCb cb));
  MOCK_METHOD(void, AbortPendingReads, (base::OnceClosure cb));
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_TEST_HELPERS_H_
