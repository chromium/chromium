// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_TEST_HELPERS_H_
#define MEDIA_FILTERS_HLS_TEST_HELPERS_H_

#include "media/filters/hls_data_source_provider.h"
#include "media/filters/hls_rendition.h"
#include "media/filters/manifest_demuxer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class FakeHlsDataSource : public HlsDataSource {
 public:
  FakeHlsDataSource(std::vector<uint8_t> data);
  ~FakeHlsDataSource() override;
  void Read(uint64_t pos,
            size_t size,
            uint8_t* buf,
            HlsDataSource::ReadCb cb) override;
  base::StringPiece GetMimeType() const override;
  void Stop() override;

 protected:
  std::vector<uint8_t> data_;
};

class FileHlsDataSource : public FakeHlsDataSource {
 public:
  FileHlsDataSource(const std::string& filename);
};

class StringHlsDataSource : public FakeHlsDataSource {
 public:
  StringHlsDataSource(base::StringPiece content);
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
               HlsDataSourceStreamManager::ReadCb cb),
              (override));

  MOCK_METHOD(hls::ParseStatus::Or<scoped_refptr<hls::MediaPlaylist>>,
              ParseMediaPlaylistFromStringSource,
              (base::StringPiece source,
               GURL uri,
               hls::types::DecimalInteger version),
              (override));

  void ReadStream(std::unique_ptr<HlsDataSourceStream> stream,
                  HlsDataSourceStreamManager::ReadCb cb) override;

 private:
  void ExchangeStreamId(HlsDataSourceStream::StreamId ticket,
                        HlsDataSourceStreamManager::ReadCb cb,
                        HlsDataSource::ReadStatus::Or<size_t> result);

  HlsDataSourceStream::StreamId::Generator stream_ticket_generator_;
  base::flat_map<HlsDataSourceStream::StreamId,
                 std::unique_ptr<HlsDataSourceStream>>
      stream_map_;
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
  MOCK_METHOD(bool, Seek, (base::TimeDelta time), (override));
  MOCK_METHOD(void, CancelPendingNetworkRequests, (), (override));
  MOCK_METHOD(absl::optional<base::TimeDelta>, GetDuration, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_TEST_HELPERS_H_
