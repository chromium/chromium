// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FAKE_TEXT_TRACK_STREAM_H_
#define MEDIA_BASE_FAKE_TEXT_TRACK_STREAM_H_

#include "base/macros.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

// Fake implementation of the DemuxerStream.  These are the stream objects
// we pass to the text renderer object when streams are added and removed.
class FakeTextTrackStream : public DemuxerStream {
 public:
  FakeTextTrackStream();
  ~FakeTextTrackStream() override;

  // DemuxerStream implementation.
  void Read(ReadCB) override;
  bool IsReadPending() const override;
  MOCK_METHOD0(audio_decoder_config, AudioDecoderConfig());
  MOCK_METHOD0(video_decoder_config, VideoDecoderConfig());
  Type type() const override;
  MOCK_METHOD0(EnableBitstreamConverter, void());
  bool SupportsConfigChanges() override;

  void SatisfyPendingRead(const base::TimeDelta& start,
                          const base::TimeDelta& duration,
                          const std::string& id,
                          const std::string& content,
                          const std::string& settings);
  void AbortPendingRead();
  void SendEosNotification();

  void Stop();

  MOCK_METHOD0(OnRead, void());

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  ReadCB read_cb_;
  bool stopping_;

  DISALLOW_COPY_AND_ASSIGN(FakeTextTrackStream);
};

}  // namespace media

#endif  // MEDIA_BASE_FAKE_TEXT_TRACK_STREAM_H_
