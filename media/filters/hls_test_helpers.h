// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_TEST_HELPERS_H_
#define MEDIA_FILTERS_HLS_TEST_HELPERS_H_

#include <optional>
#include <string_view>
#include <tuple>
#include <vector>

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
  MOCK_METHOD(const std::string&, GetMimeType, (), (const, override));
  MOCK_METHOD(void,
              Initialize,
              (base::OnceCallback<void(bool)> init_cb),
              (override));

  // Mocked methods from DataSource
  MOCK_METHOD(void,
              Read,
              (int64_t position,
               base::span<uint8_t> data,
               DataSource::ReadCB read_cb),
              (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, Abort, (), (override));
  MOCK_METHOD(bool, GetSize, (int64_t * size_out), (override));
  MOCK_METHOD(bool, IsStreaming, (), (const, override));
  MOCK_METHOD(void, SetBitrate, (int bitrate), (override));
  MOCK_METHOD(bool, PassedTimingAllowOriginCheck, (), (override));
  MOCK_METHOD(bool, WouldTaintOrigin, (), (const, override));
  MOCK_METHOD(bool, AssumeFullyBuffered, (), (const, override));
  MOCK_METHOD(int64_t, GetMemoryUsage, (), (override));
  MOCK_METHOD(void, SetPreload, (DataSource::Preload preload), (override));
  MOCK_METHOD(bool, DidRedirect, (), (const, override));
  MOCK_METHOD(GURL, GetUrlAfterRedirects, (), (const, override));
  MOCK_METHOD(GURL, GetUrlDataOrigin, (), (const, override));
  MOCK_METHOD(void, StopPreloading, (), (override));
  MOCK_METHOD(void,
              OnMediaPlaybackRateChanged,
              (double playback_rate),
              (override));
  MOCK_METHOD(void, OnMediaIsPlaying, (), (override));
};

class MockHlsDataSourceProvider : public HlsDataSourceProvider {
 public:
  MockHlsDataSourceProvider();
  ~MockHlsDataSourceProvider() override;
  MOCK_METHOD(void,
              ReadFromUrl,
              (HlsDataSourceProvider::UrlDataSegment,
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
  MOCK_METHOD(
      void,
      ReadMediaSegment,
      (const hls::MediaSegment&, bool, bool, HlsDataSourceProvider::ReadCb),
      (override));

  MOCK_METHOD(void,
              UpdateRenditionManifestUri,
              (std::string, GURL, HlsDemuxerStatusCallback),
              (override));

  MOCK_METHOD(void, Quit, (HlsDemuxerStatus), (override));

  MOCK_METHOD(void, UpdateNetworkSpeed, (uint64_t), (override));

  MOCK_METHOD(void, SetEndOfStream, (bool), (override));
};

class MockHlsRendition : public HlsRendition {
 public:
  explicit MockHlsRendition(GURL uri = GURL("https://hls.io/manifest.m3u8"));
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
              (scoped_refptr<hls::MediaPlaylist>),
              (override));
  MOCK_METHOD(void, MockUpdatePlaylistURI, (const GURL&), ());
  void UpdatePlaylistURI(const GURL& playlist_uri) override;
  const GURL& MediaPlaylistUri() const override;

 private:
  GURL uri_;
};

class StringHlsDataSourceStreamFactory {
 public:
  static std::unique_ptr<HlsDataSourceStream> CreateStream(
      std::string content,
      std::optional<hls::SecurityMetadata> info = std::nullopt,
      GURL uri = GURL("http://test-only-data-sources.com"));
};

class FileHlsDataSourceStreamFactory {
 public:
  static std::unique_ptr<HlsDataSourceStream> CreateStream(
      std::string file,
      std::optional<hls::SecurityMetadata> info = std::nullopt,
      GURL uri = GURL("http://test-only-data-sources.com"));
};

class MockDataSourceFactory : public CrossOriginDataSource::Factory {
 public:
  // Represents the behavior of a data source when it is created.
  struct DataSourceBehavior {
    DataSourceBehavior();
    ~DataSourceBehavior();
    DataSourceBehavior(const DataSourceBehavior&);
    DataSourceBehavior(DataSourceBehavior&&);
    DataSourceBehavior& operator=(const DataSourceBehavior&);
    DataSourceBehavior& operator=(DataSourceBehavior&&);

    // If false, Initialize() will fail.
    bool does_connect = true;

    // Value returned by WouldTaintOrigin().
    bool would_taint_origin = false;

    // The expected original URI. Used for matching.
    std::optional<GURL> original_uri;

    // If set, DidRedirect() will return true and GetUrlAfterRedirects()
    // will return this URI.
    std::optional<std::string> redirect_uri;

    // Read expectations to apply to this data source.
    // Tuple: (position, size, response_size)
    std::vector<std::tuple<size_t, size_t, int>> read_expectations;
  };

  ~MockDataSourceFactory() override;
  MockDataSourceFactory();

  void Create(const GURL& uri,
              DataSource::CacheMode cache_mode,
              DataSource::EncodingMode encoding_mode,
              base::OnceCallback<void(std::unique_ptr<CrossOriginDataSource>)>
                  cb) override;

  // The Setup mock method allows tests to intercept data source creation and
  // configure the mock reactively.
  //
  // Example (Reactive Configuration):
  //
  //   EXPECT_CALL(factory, Setup(_, GURL("http://example.com"), _, _))
  //       .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
  //           MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
  //           EXPECT_CALL(*mock, Read(0, SpanSizeEq(1024), _))
  //               .WillOnce(RunOnceCallback<2>(1024));
  //       });
  MOCK_METHOD(void,
              Setup,
              (MockDataSource * mock,
               const GURL& uri,
               DataSource::CacheMode cache_mode,
               DataSource::EncodingMode encoding_mode));

  // Register expected behavior for a URL.
  // The factory will automatically apply this behavior when a data source
  // for the GURL is created, unless Setup is explicitly overridden for it.
  //
  // Example (Declarative Configuration):
  //
  //   MockDataSourceFactory::DataSourceBehavior behavior;
  //   behavior.original_uri = GURL("http://example.com");
  //   behavior.read_expectations = {{0, 1024, 1024}};
  //   factory.Expect(std::move(behavior));
  void Expect(DataSourceBehavior behavior);

  // Simple API: Add read expectation for the next created data source.
  //
  // Example (Simple Configuration):
  //
  //   factory.AddReadExpectation(0, 1024, 1024);
  //   factory.AddReadExpectation(1024, 1024, 0);
  void AddReadExpectation(size_t from, size_t to, int response);

  // Simple API: Add read expectation for a specific URL.
  //
  // Example (URL-specific Simple Configuration):
  //
  //   factory.AddReadExpectation(GURL("http://example.com"), 0, 1024, 1024);
  void AddReadExpectation(const GURL& url,
                          size_t from,
                          size_t to,
                          int response);

  // Helper static methods to configure a mock with standard behaviors.
  // Useful when overriding Setup in tests.
  static void ConfigureAsSuccess(MockDataSource* mock, const GURL& uri);
  static void ConfigureAsFailure(MockDataSource* mock);
  static void ConfigureAsRedirect(MockDataSource* mock, const GURL& target_uri);

 private:
  // Default implementation for Setup, which applies registered behaviors
  // and simple expectations.
  void DefaultSetup(MockDataSource* mock,
                    const GURL& uri,
                    DataSource::CacheMode cache_mode,
                    DataSource::EncodingMode encoding_mode);

  std::vector<DataSourceBehavior> expected_behaviors_;
  std::vector<std::tuple<size_t, size_t, int>> global_read_expectations_;
  std::vector<std::pair<GURL, std::tuple<size_t, size_t, int>>>
      url_read_expectations_;
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

MATCHER_P(SpanSizeEq, expected, "") {
  return arg.size() == base::checked_cast<size_t>(expected);
}

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_TEST_HELPERS_H_
