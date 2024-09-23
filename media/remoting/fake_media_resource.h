// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_FAKE_MEDIA_RESOURCE_H_
#define MEDIA_REMOTING_FAKE_MEDIA_RESOURCE_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
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

  FakeDemuxerStream(const FakeDemuxerStream&) = delete;
  FakeDemuxerStream& operator=(const FakeDemuxerStream&) = delete;

  ~FakeDemuxerStream() override;

  // DemuxerStream implementation.
  MOCK_METHOD2(Read, void(uint32_t count, ReadCB read_cb));
  void FakeRead(uint32_t count, ReadCB read_cb);
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  Type type() const override;
  StreamLiveness liveness() const override;
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
};

// Audio only demuxer stream provider
class FakeMediaResource final : public MediaResource {
 public:
  FakeMediaResource();

  FakeMediaResource(const FakeMediaResource&) = delete;
  FakeMediaResource& operator=(const FakeMediaResource&) = delete;

  ~FakeMediaResource() override;

  // MediaResource implementation.
  std::vector<DemuxerStream*> GetAllStreams() override;

 private:
  std::unique_ptr<FakeDemuxerStream> audio_stream_;
  std::unique_ptr<FakeDemuxerStream> video_stream_;
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_FAKE_MEDIA_RESOURCE_H_
