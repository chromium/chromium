// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FAKE_DEMUXER_STREAM_H_
#define MEDIA_BASE_FAKE_DEMUXER_STREAM_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_resource.h"
#include "media/base/video_decoder_config.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

class FakeDemuxerStream : public DemuxerStream {
 public:
  // Constructs an object that outputs |num_configs| different configs in
  // sequence with |num_frames_in_one_config| buffers for each config. The
  // output buffers are encrypted if |is_encrypted| is true.
  FakeDemuxerStream(int num_configs,
                    int num_buffers_in_one_config,
                    bool is_encrypted);
  ~FakeDemuxerStream() override;

  // DemuxerStream implementation.
  void Read(ReadCB read_cb) override;
  bool IsReadPending() const override;
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  Type type() const override;
  bool SupportsConfigChanges() override;

  void Initialize();

  int num_buffers_returned() const { return num_buffers_returned_; }

  // Upon the next read, holds the read callback until SatisfyRead() or Reset()
  // is called.
  void HoldNextRead();

  // Upon the next config change read, holds the read callback until
  // SatisfyRead() or Reset() is called. If there is no config change any more,
  // no read will be held.
  void HoldNextConfigChangeRead();

  // Satisfies the pending read with the next scheduled status and buffer.
  void SatisfyRead();

  // Satisfies pending read request and then holds the following read.
  void SatisfyReadAndHoldNext();

  // Satisfies the pending read (if any) with kAborted and NULL. This call
  // always clears |hold_next_read_|.
  void Reset();

  // Satisfies the pending read (if any) with kError and NULL. This call
  // always clears |hold_next_read_|.
  void Error();

  // Reset() this demuxer stream and set the reading position to the start of
  // the stream.
  void SeekToStart();

  // Sets further read requests to return EOS buffers.
  void SeekToEndOfStream();

  base::TimeDelta duration() const { return duration_; }

 private:
  void UpdateVideoDecoderConfig();
  void DoRead();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  const int num_configs_;
  const int num_buffers_in_one_config_;
  const bool config_changes_;
  const bool is_encrypted_;

  int num_configs_left_;

  // Number of frames left with the current decoder config.
  int num_buffers_left_in_current_config_;

  int num_buffers_returned_;

  base::TimeDelta current_timestamp_;
  base::TimeDelta duration_;

  gfx::Size next_coded_size_;
  VideoDecoderConfig video_decoder_config_;

  ReadCB read_cb_;

  int next_read_num_;
  // Zero-based number indicating which read operation should be held. -1 means
  // no read shall be held.
  int read_to_hold_;

  DISALLOW_COPY_AND_ASSIGN(FakeDemuxerStream);
};

class FakeMediaResource : public MediaResource {
 public:
  // Note: FakeDemuxerStream currently only supports a fake video DemuxerStream.
  FakeMediaResource(int num_video_configs,
                    int num_video_buffers_in_one_config,
                    bool is_video_encrypted);
  ~FakeMediaResource() override;

  // MediaResource implementation.
  std::vector<DemuxerStream*> GetAllStreams() override;

 private:
  FakeDemuxerStream fake_video_stream_;

  DISALLOW_COPY_AND_ASSIGN(FakeMediaResource);
};

}  // namespace media

#endif  // MEDIA_BASE_FAKE_DEMUXER_STREAM_H_
