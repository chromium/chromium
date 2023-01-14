// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/mojo/clients/mojo_demuxer_stream_impl.h"
#include "media/mojo/services/mojo_demuxer_stream_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Invoke;

namespace media {

// This test both MojoDemuxerStreamImpl and MojoDemuxerStreamAdapter.
class MojoDemuxerStreamAdapterTest : public testing::Test {
 public:
  MojoDemuxerStreamAdapterTest() {
    task_runner_ = task_environment_.GetMainThreadTaskRunner();
  }

  void Initialize(DemuxerStream::Type StreamType) {
    base::RunLoop init_loop;
    // wait initialize finish.
    auto init_done_cb = base::BindLambdaForTesting([&]() {
      is_stream_ready_ = true;
      init_loop.QuitWhenIdle();
    });

    stream_ = std::make_unique<MockDemuxerStream>(StreamType);
    if (StreamType == DemuxerStream::Type::AUDIO)
      stream_->set_audio_decoder_config(TestAudioConfig::Normal());
    else if (StreamType == DemuxerStream::Type::VIDEO)
      stream_->set_video_decoder_config(TestVideoConfig::Normal());

    mojo::PendingRemote<mojom::DemuxerStream> stream_remote;
    mojo_client_stream_ = std::make_unique<MojoDemuxerStreamImpl>(
        stream_.get(), stream_remote.InitWithNewPipeAndPassReceiver());

    mojo_stream_adapter_ = std::make_unique<MojoDemuxerStreamAdapter>(
        std::move(stream_remote), std::move(init_done_cb));
    init_loop.Run();
  }

  void ReadBuffer(int count, DemuxerStream::ReadCB done_cb) {
    EXPECT_TRUE(is_stream_ready_);
    EXPECT_GT(count, 0);
    mojo_stream_adapter_->Read(count, std::move(done_cb));
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<MockDemuxerStream> stream_;

  bool is_stream_ready_ = false;
  std::unique_ptr<MojoDemuxerStreamImpl> mojo_client_stream_;
  std::unique_ptr<MojoDemuxerStreamAdapter> mojo_stream_adapter_;
};

TEST_F(MojoDemuxerStreamAdapterTest, InitializeAudioStream) {
  Initialize(DemuxerStream::Type::AUDIO);

  mojo_stream_adapter_->audio_decoder_config();
  EXPECT_EQ(mojo_stream_adapter_->type(), DemuxerStream::Type::AUDIO);
  EXPECT_TRUE(mojo_stream_adapter_->SupportsConfigChanges());
}

TEST_F(MojoDemuxerStreamAdapterTest, InitializeAudioStreamAndReadMultiBuffer) {
  Initialize(DemuxerStream::Type::AUDIO);

  {
    base::RunLoop success_read_loop;
    DemuxerStream::DecoderBufferVector buffers;
    // Requested 200 but just return 100 buffer.
    for (int i = 0; i < 100; ++i) {
      buffers.emplace_back(base::MakeRefCounted<DecoderBuffer>(12));
    }
    EXPECT_CALL(*stream_, OnRead(_))
        .WillOnce(RunOnceCallback<0>(DemuxerStream::Status::kOk, buffers));

    auto done_cb = base::BindLambdaForTesting(
        [&](DemuxerStream::Status status,
            DemuxerStream::DecoderBufferVector buffers) {
          EXPECT_EQ(status, DemuxerStream::Status::kOk);
          EXPECT_EQ(buffers.size(), 100u);
          success_read_loop.QuitWhenIdle();
        });
    ReadBuffer(200, done_cb);
    success_read_loop.Run();
  }

  {
    base::RunLoop config_changed_read_loop;
    EXPECT_CALL(*stream_, OnRead(_))
        .WillOnce(RunOnceCallback<0>(DemuxerStream::Status::kConfigChanged,
                                     DemuxerStream::DecoderBufferVector()));

    auto done_cb = base::BindLambdaForTesting(
        [&](DemuxerStream::Status status,
            DemuxerStream::DecoderBufferVector buffers) {
          EXPECT_EQ(status, DemuxerStream::Status::kConfigChanged);
          EXPECT_TRUE(buffers.empty());
          config_changed_read_loop.QuitWhenIdle();
        });
    ReadBuffer(200, done_cb);
    config_changed_read_loop.Run();
  }

  {
    base::RunLoop abort_read_loop;
    EXPECT_CALL(*stream_, OnRead(_))
        .WillOnce(RunOnceCallback<0>(DemuxerStream::Status::kAborted,
                                     DemuxerStream::DecoderBufferVector()));

    auto done_cb = base::BindLambdaForTesting(
        [&](DemuxerStream::Status status,
            DemuxerStream::DecoderBufferVector buffers) {
          EXPECT_EQ(status, DemuxerStream::Status::kAborted);
          EXPECT_TRUE(buffers.empty());
          abort_read_loop.QuitWhenIdle();
        });
    ReadBuffer(200, done_cb);
    abort_read_loop.Run();
  }
}

TEST_F(MojoDemuxerStreamAdapterTest, InitializeAudioStreamAndReadOneBuffer) {
  Initialize(DemuxerStream::Type::AUDIO);

  {
    base::RunLoop success_read_loop;
    DemuxerStream::DecoderBufferVector buffers;
    buffers.emplace_back(base::MakeRefCounted<DecoderBuffer>(12));
    EXPECT_CALL(*stream_, OnRead(_))
        .WillOnce(RunOnceCallback<0>(DemuxerStream::Status::kOk, buffers));

    auto done_cb = base::BindLambdaForTesting(
        [&](DemuxerStream::Status status,
            DemuxerStream::DecoderBufferVector buffers) {
          EXPECT_EQ(status, DemuxerStream::Status::kOk);
          success_read_loop.QuitWhenIdle();
        });
    ReadBuffer(1, done_cb);
    success_read_loop.Run();
  }

  {
    base::RunLoop config_changed_read_loop;
    EXPECT_CALL(*stream_, OnRead(_))
        .WillOnce(RunOnceCallback<0>(DemuxerStream::Status::kConfigChanged,
                                     DemuxerStream::DecoderBufferVector()));

    auto done_cb = base::BindLambdaForTesting(
        [&](DemuxerStream::Status status,
            DemuxerStream::DecoderBufferVector buffers) {
          EXPECT_EQ(status, DemuxerStream::Status::kConfigChanged);
          EXPECT_TRUE(buffers.empty());
          config_changed_read_loop.QuitWhenIdle();
        });
    ReadBuffer(1, done_cb);
    config_changed_read_loop.Run();
  }

  {
    base::RunLoop abort_read_loop;
    EXPECT_CALL(*stream_, OnRead(_))
        .WillOnce(RunOnceCallback<0>(DemuxerStream::Status::kAborted,
                                     DemuxerStream::DecoderBufferVector()));

    auto done_cb = base::BindLambdaForTesting(
        [&](DemuxerStream::Status status,
            DemuxerStream::DecoderBufferVector buffers) {
          EXPECT_EQ(status, DemuxerStream::Status::kAborted);
          EXPECT_TRUE(buffers.empty());
          abort_read_loop.QuitWhenIdle();
        });
    ReadBuffer(1, done_cb);
    abort_read_loop.Run();
  }
}

TEST_F(MojoDemuxerStreamAdapterTest, InitializeVideoStream) {
  Initialize(DemuxerStream::Type::VIDEO);

  mojo_stream_adapter_->video_decoder_config();
  EXPECT_EQ(mojo_stream_adapter_->type(), DemuxerStream::Type::VIDEO);
  EXPECT_TRUE(mojo_stream_adapter_->SupportsConfigChanges());
}

TEST_F(MojoDemuxerStreamAdapterTest, EnableBitstreamConverter) {
  Initialize(DemuxerStream::Type::VIDEO);

  base::RunLoop loop;
  EXPECT_CALL(*stream_, EnableBitstreamConverter());
  mojo_stream_adapter_->EnableBitstreamConverter();
  loop.RunUntilIdle();
}

}  // namespace media
