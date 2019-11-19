// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_FAKE_MEDIA_RESOURCE_H_
#define MEDIA_REMOTING_FAKE_MEDIA_RESOURCE_H_

#include "base/containers/circular_deque.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_resource.h"
#include "media/base/video_decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace remoting {

class FakeDemuxerStream : public DemuxerStream {
 public:
  explicit FakeDemuxerStream(bool is_audio);
  ~FakeDemuxerStream() override;

  // DemuxerStream implementation.
  MOCK_METHOD1(Read, void(ReadCB read_cb));
  void FakeRead(ReadCB read_cb);
  bool IsReadPending() const override;
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  Type type() const override;
  Liveness liveness() const override;
  void EnableBitstreamConverter() override {}
  bool SupportsConfigChanges() override;

  void CreateFakeFrame(size_t size, bool key_frame, int pts_ms);

 private:
  using BufferQueue = base::circular_deque<scoped_refptr<DecoderBuffer>>;
  BufferQueue buffer_queue_;
  ReadCB pending_read_cb_;
  Type type_;
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

  DISALLOW_COPY_AND_ASSIGN(FakeDemuxerStream);
};

// Audio only demuxer stream provider
class FakeMediaResource : public MediaResource {
 public:
  FakeMediaResource();
  ~FakeMediaResource() final;

  // MediaResource implementation.
  std::vector<DemuxerStream*> GetAllStreams() override;

 private:
  std::unique_ptr<FakeDemuxerStream> demuxer_stream_;

  DISALLOW_COPY_AND_ASSIGN(FakeMediaResource);
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_FAKE_MEDIA_RESOURCE_H_
