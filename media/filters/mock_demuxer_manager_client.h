// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_MOCK_DEMUXER_MANAGER_CLIENT_H_
#define MEDIA_FILTERS_MOCK_DEMUXER_MANAGER_CLIENT_H_

#include <stdint.h>

#include <vector>

#include "base/time/time.h"
#include "media/base/eme_constants.h"
#include "media/base/media_track.h"
#include "media/base/pipeline_status.h"
#include "media/filters/demuxer_manager.h"
#include "media/media_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace media {

class MockDemuxerManagerClient : public DemuxerManager::Client {
 public:
  MockDemuxerManagerClient();

  MockDemuxerManagerClient(const MockDemuxerManagerClient&) = delete;
  MockDemuxerManagerClient& operator=(const MockDemuxerManagerClient&) = delete;

  ~MockDemuxerManagerClient() override;

  // TrackManager implementation.
  MOCK_METHOD(void, AddTrack, (const MediaTrack&), (override));
  MOCK_METHOD(void, RemoveTrack, (const MediaTrack&), (override));
  MOCK_METHOD(void,
              SetTrackState,
              (const MediaTrack&, MediaTrack::State),
              (override));

  // DemuxerManager::Client implementation.
  MOCK_METHOD(void,
              OnEncryptedMediaInitData,
              (EmeInitDataType, const std::vector<uint8_t>&),
              (override));
  MOCK_METHOD(void, OnChunkDemuxerOpened, (ChunkDemuxer*), (override));
  MOCK_METHOD(void, OnProgress, (), (override));
  MOCK_METHOD(void, OnError, (PipelineStatus), (override));
  MOCK_METHOD(void, StopForDemuxerReset, (), (override));
  MOCK_METHOD(void, RestartForHls, (), (override));
#if BUILDFLAG(ENABLE_HLS_DEMUXER)
  MOCK_METHOD(base::SequenceBound<HlsDataSourceProvider>,
              GetHlsDataSourceProvider,
              (),
              (override));
#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)
  MOCK_METHOD(bool, CouldPlayIfEnoughData, (), (override));
  MOCK_METHOD(void, MakeDemuxerThreadDumper, (Demuxer*), (override));
  MOCK_METHOD(double, CurrentTime, (), (const, override));
  MOCK_METHOD(void, UpdateLoadedUrl, (const GURL&), (override));
  MOCK_METHOD(void, DemuxerRequestsSeek, (base::TimeDelta), (override));
};

}  // namespace media

#endif  // MEDIA_FILTERS_MOCK_DEMUXER_MANAGER_CLIENT_H_
