// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_MOCK_RECEIVER_CONTROLLER_H_
#define MEDIA_REMOTING_MOCK_RECEIVER_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/remoting/receiver_controller.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MojoDecoderBufferWriter;

namespace remoting {

class MockRemotee : public mojom::Remotee {
 public:
  MockRemotee();
  ~MockRemotee() override;

  void BindMojoReceiver(mojo::PendingReceiver<Remotee> receiver);

  void SendAudioFrame(uint32_t frame_count,
                      scoped_refptr<DecoderBuffer> buffer);
  void SendVideoFrame(uint32_t frame_count,
                      scoped_refptr<DecoderBuffer> buffer);

  // mojom::Remotee implementation
  void OnRemotingSinkReady(
      mojo::PendingRemote<mojom::RemotingSink> remoting_sink) override;
  void SendMessageToSource(const std::vector<uint8_t>& message) override;
  void StartDataStreams(
      mojo::PendingRemote<mojom::RemotingDataStreamReceiver> audio_stream,
      mojo::PendingRemote<mojom::RemotingDataStreamReceiver> video_stream)
      override;
  void OnFlushUntil(uint32_t audio_count, uint32_t video_count) override;
  void OnVideoNaturalSizeChange(const gfx::Size& size) override;

  void Reset();

  gfx::Size changed_size() { return changed_size_; }
  uint32_t flush_audio_count() { return flush_audio_count_; }
  uint32_t flush_video_count() { return flush_video_count_; }

  mojo::PendingRemote<mojom::Remotee> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  mojo::Remote<mojom::RemotingDataStreamReceiver> audio_stream_;
  mojo::Remote<mojom::RemotingDataStreamReceiver> video_stream_;

 private:
  gfx::Size changed_size_;

  uint32_t flush_audio_count_{0};
  uint32_t flush_video_count_{0};

  std::unique_ptr<MojoDecoderBufferWriter> audio_buffer_writer_;
  std::unique_ptr<MojoDecoderBufferWriter> video_buffer_writer_;

  mojo::Remote<mojom::RemotingSink> remoting_sink_;
  mojo::Receiver<mojom::Remotee> receiver_{this};
};

class MockReceiverController : public ReceiverController {
 public:
  static MockReceiverController* GetInstance();

  MockRemotee* mock_remotee() { return mock_remotee_.get(); }

 private:
  friend base::NoDestructor<MockReceiverController>;
  friend testing::StrictMock<MockReceiverController>;
  friend testing::NiceMock<MockReceiverController>;

  MockReceiverController();
  ~MockReceiverController() override;

  void OnSendRpc(std::vector<uint8_t> message);

  std::unique_ptr<MockRemotee> mock_remotee_;
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_MOCK_RECEIVER_CONTROLLER_H_
