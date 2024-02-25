// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef MEDIA_BASE_MOCK_DEMUXER_HOST_H_
#define MEDIA_BASE_MOCK_DEMUXER_HOST_H_

#include "media/base/demuxer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockDemuxerHost : public DemuxerHost {
 public:
  MockDemuxerHost();

  MockDemuxerHost(const MockDemuxerHost&) = delete;
  MockDemuxerHost& operator=(const MockDemuxerHost&) = delete;

  ~MockDemuxerHost() override;

  MOCK_METHOD1(OnBufferedTimeRangesChanged,
               void(const Ranges<base::TimeDelta>&));
  MOCK_METHOD1(SetDuration, void(base::TimeDelta duration));
  MOCK_METHOD1(OnDemuxerError, void(PipelineStatus error));
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_DEMUXER_HOST_H_
