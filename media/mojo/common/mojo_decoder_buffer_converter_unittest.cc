// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/mojo_decoder_buffer_converter.h"

#include <stdint.h>

#include <memory>

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

uint32_t kDefaultDataPipeCapacityBytes = 1024;

MATCHER_P(MatchesDecoderBuffer, buffer, "") {
  DCHECK(arg);
  return arg->MatchesForTesting(*buffer);
}

class MojoDecoderBufferConverter {
 public:
  MojoDecoderBufferConverter(
      uint32_t data_pipe_capacity_bytes = kDefaultDataPipeCapacityBytes) {
    mojo::DataPipe data_pipe(data_pipe_capacity_bytes);

    writer = std::make_unique<MojoDecoderBufferWriter>(
        std::move(data_pipe.producer_handle));
    reader = std::make_unique<MojoDecoderBufferReader>(
        std::move(data_pipe.consumer_handle));
  }

  void ConvertAndVerify(scoped_refptr<DecoderBuffer> media_buffer) {
    base::RunLoop run_loop;
    base::MockCallback<MojoDecoderBufferReader::ReadCB> mock_cb;
    EXPECT_CALL(mock_cb, Run(MatchesDecoderBuffer(media_buffer)))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

    mojom::DecoderBufferPtr mojo_buffer =
        writer->WriteDecoderBuffer(media_buffer);
    reader->ReadDecoderBuffer(std::move(mojo_buffer), mock_cb.Get());
    run_loop.Run();
  }

  std::unique_ptr<MojoDecoderBufferWriter> writer;
  std::unique_ptr<MojoDecoderBufferReader> reader;
};

}  // namespace

TEST(MojoDecoderBufferConverterTest, ConvertDecoderBuffer_Normal) {
  base::test::SingleThreadTaskEnvironment task_environment;
  const uint8_t kData[] = "hello, world";
  const uint8_t kSideData[] = "sideshow bob";
  const size_t kDataSize = base::size(kData);
  const size_t kSideDataSize = base::size(kSideData);

  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&kData), kDataSize,
      reinterpret_cast<const uint8_t*>(&kSideData), kSideDataSize));
  buffer->set_timestamp(base::TimeDelta::FromMilliseconds(123));
  buffer->set_duration(base::TimeDelta::FromMilliseconds(456));
  buffer->set_discard_padding(
      DecoderBuffer::DiscardPadding(base::TimeDelta::FromMilliseconds(5),
                                    base::TimeDelta::FromMilliseconds(6)));

  MojoDecoderBufferConverter converter;
  converter.ConvertAndVerify(buffer);
}

TEST(MojoDecoderBufferConverterTest, ConvertDecoderBuffer_EOS) {
  base::test::SingleThreadTaskEnvironment task_environment;
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CreateEOSBuffer());

  MojoDecoderBufferConverter converter;
  converter.ConvertAndVerify(buffer);
}

// TODO(xhwang): Investigate whether we can get rid of zero-byte-buffer.
// See http://crbug.com/663438
TEST(MojoDecoderBufferConverterTest, ConvertDecoderBuffer_ZeroByteBuffer) {
  base::test::SingleThreadTaskEnvironment task_environment;
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(0));

  MojoDecoderBufferConverter converter;
  converter.ConvertAndVerify(buffer);
}

TEST(MojoDecoderBufferConverterTest, ConvertDecoderBuffer_KeyFrame) {
  base::test::SingleThreadTaskEnvironment task_environment;
  const uint8_t kData[] = "hello, world";
  const size_t kDataSize = base::size(kData);

  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&kData), kDataSize));
  buffer->set_is_key_frame(true);
  EXPECT_TRUE(buffer->is_key_frame());

  MojoDecoderBufferConverter converter;
  converter.ConvertAndVerify(buffer);
}

TEST(MojoDecoderBufferConverterTest, ConvertDecoderBuffer_EncryptedBuffer) {
  base::test::SingleThreadTaskEnvironment task_environment;
  const uint8_t kData[] = "hello, world";
  const size_t kDataSize = base::size(kData);
  const char kKeyId[] = "00112233445566778899aabbccddeeff";
  const char kIv[] = "0123456789abcdef";

  std::vector<SubsampleEntry> subsamples;
  subsamples.push_back(SubsampleEntry(10, 20));
  subsamples.push_back(SubsampleEntry(30, 40));
  subsamples.push_back(SubsampleEntry(50, 60));

  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&kData), kDataSize));
  buffer->set_decrypt_config(
      DecryptConfig::CreateCencConfig(kKeyId, kIv, subsamples));
  {
    MojoDecoderBufferConverter converter;
    converter.ConvertAndVerify(buffer);
  }

  // Test 'cbcs'.
  buffer->set_decrypt_config(DecryptConfig::CreateCbcsConfig(
      kKeyId, kIv, subsamples, EncryptionPattern(5, 6)));
  {
    MojoDecoderBufferConverter converter;
    converter.ConvertAndVerify(buffer);
  }

  // Test unencrypted. This is used for clear buffer in an encrypted stream.
  buffer->set_decrypt_config(nullptr);
  {
    MojoDecoderBufferConverter converter;
    converter.ConvertAndVerify(buffer);
  }
}

// This test verifies that a DecoderBuffer larger than data-pipe capacity
// can be transmitted properly.
TEST(MojoDecoderBufferConverterTest, Chunked) {
  base::test::SingleThreadTaskEnvironment task_environment;
  const uint8_t kData[] = "Lorem ipsum dolor sit amet, consectetur cras amet";
  const size_t kDataSize = base::size(kData);
  scoped_refptr<DecoderBuffer> buffer =
      DecoderBuffer::CopyFrom(kData, kDataSize);

  MojoDecoderBufferConverter converter(kDataSize / 3);
  converter.ConvertAndVerify(buffer);
}

// This test verifies that MojoDecoderBufferReader::ReadCB is called with a
// NULL DecoderBuffer if data pipe is closed during transmission.
TEST(MojoDecoderBufferConverterTest, WriterSidePipeError) {
  base::test::SingleThreadTaskEnvironment task_environment;
  const uint8_t kData[] = "Lorem ipsum dolor sit amet, consectetur cras amet";
  const size_t kDataSize = base::size(kData);
  scoped_refptr<DecoderBuffer> media_buffer =
      DecoderBuffer::CopyFrom(kData, kDataSize);

  // Verify that ReadCB is called with a NULL decoder buffer.
  base::RunLoop run_loop;
  base::MockCallback<MojoDecoderBufferReader::ReadCB> mock_cb;
  EXPECT_CALL(mock_cb, Run(testing::IsNull()))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // Make data pipe with capacity smaller than decoder buffer so that only
  // partial data is written.
  MojoDecoderBufferConverter converter(kDataSize / 2);
  mojom::DecoderBufferPtr mojo_buffer =
      converter.writer->WriteDecoderBuffer(media_buffer);
  converter.reader->ReadDecoderBuffer(std::move(mojo_buffer), mock_cb.Get());

  // Before the entire data is transmitted, close the handle on writer side.
  // The reader side will get notified and report the error.
  converter.writer.reset();
  run_loop.Run();
}

// This test verifies that MojoDecoderBuffer supports concurrent writes and
// reads.
TEST(MojoDecoderBufferConverterTest, ConcurrentDecoderBuffers) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  // Prevent all of the buffers from fitting at once to exercise the chunking
  // logic.
  MojoDecoderBufferConverter converter(4);

  // Three buffers: normal, EOS, normal.
  const uint8_t kData[] = "Hello, world";
  const size_t kDataSize = base::size(kData);
  scoped_refptr<DecoderBuffer> media_buffer1 =
      DecoderBuffer::CopyFrom(kData, kDataSize);
  scoped_refptr<DecoderBuffer> media_buffer2(DecoderBuffer::CreateEOSBuffer());
  scoped_refptr<DecoderBuffer> media_buffer3 =
      DecoderBuffer::CopyFrom(kData, kDataSize);

  // Expect the read callbacks to be issued in the same order.
  ::testing::InSequence scoper;
  base::MockCallback<MojoDecoderBufferReader::ReadCB> mock_cb1;
  base::MockCallback<MojoDecoderBufferReader::ReadCB> mock_cb2;
  base::MockCallback<MojoDecoderBufferReader::ReadCB> mock_cb3;
  EXPECT_CALL(mock_cb1, Run(MatchesDecoderBuffer(media_buffer1)));
  EXPECT_CALL(mock_cb2, Run(MatchesDecoderBuffer(media_buffer2)));
  EXPECT_CALL(mock_cb3, Run(MatchesDecoderBuffer(media_buffer3)))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // Write all of the buffers at once.
  mojom::DecoderBufferPtr mojo_buffer1 =
      converter.writer->WriteDecoderBuffer(media_buffer1);
  mojom::DecoderBufferPtr mojo_buffer2 =
      converter.writer->WriteDecoderBuffer(media_buffer2);
  mojom::DecoderBufferPtr mojo_buffer3 =
      converter.writer->WriteDecoderBuffer(media_buffer3);

  // Read all of the buffers at once.
  // Technically could be satisfied by ReadDecoderBuffer() blocking, but that's
  // actually a valid implementation. (Quitting the |run_loop| won't work
  // properly with that setup though.)
  converter.reader->ReadDecoderBuffer(std::move(mojo_buffer1), mock_cb1.Get());
  converter.reader->ReadDecoderBuffer(std::move(mojo_buffer2), mock_cb2.Get());
  converter.reader->ReadDecoderBuffer(std::move(mojo_buffer3), mock_cb3.Get());

  run_loop.Run();
}

TEST(MojoDecoderBufferConverterTest, FlushWithoutRead) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  base::MockCallback<base::OnceClosure> mock_flush_cb;
  EXPECT_CALL(mock_flush_cb, Run());

  MojoDecoderBufferConverter converter;
  converter.reader->Flush(mock_flush_cb.Get());

  run_loop.RunUntilIdle();
}

TEST(MojoDecoderBufferConverterTest, FlushAfterRead) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  const uint8_t kData[] = "Lorem ipsum dolor sit amet, consectetur cras amet";
  const size_t kDataSize = base::size(kData);
  scoped_refptr<DecoderBuffer> media_buffer =
      DecoderBuffer::CopyFrom(kData, kDataSize);

  MojoDecoderBufferConverter converter(kDataSize / 3);
  converter.ConvertAndVerify(media_buffer);

  base::MockCallback<base::OnceClosure> mock_flush_cb;
  EXPECT_CALL(mock_flush_cb, Run())
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  converter.reader->Flush(mock_flush_cb.Get());

  run_loop.Run();
}

TEST(MojoDecoderBufferConverterTest, FlushBeforeRead) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  const uint8_t kData[] = "Lorem ipsum dolor sit amet, consectetur cras amet";
  const size_t kDataSize = base::size(kData);
  scoped_refptr<DecoderBuffer> media_buffer =
      DecoderBuffer::CopyFrom(kData, kDataSize);

  MojoDecoderBufferConverter converter;

  base::MockCallback<MojoDecoderBufferReader::ReadCB> mock_read_cb;
  base::MockCallback<base::OnceClosure> mock_flush_cb;

  ::testing::InSequence sequence;
  EXPECT_CALL(mock_flush_cb, Run());
  EXPECT_CALL(mock_read_cb, Run(MatchesDecoderBuffer(media_buffer)))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // Write, Flush, then Read.
  mojom::DecoderBufferPtr mojo_buffer =
      converter.writer->WriteDecoderBuffer(media_buffer);
  converter.reader->Flush(mock_flush_cb.Get());
  converter.reader->ReadDecoderBuffer(std::move(mojo_buffer),
                                      mock_read_cb.Get());
  run_loop.Run();
}

TEST(MojoDecoderBufferConverterTest, FlushBeforeChunkedRead) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  const uint8_t kData[] = "Lorem ipsum dolor sit amet, consectetur cras amet";
  const size_t kDataSize = base::size(kData);
  scoped_refptr<DecoderBuffer> media_buffer =
      DecoderBuffer::CopyFrom(kData, kDataSize);

  MojoDecoderBufferConverter converter(kDataSize / 3);

  base::MockCallback<MojoDecoderBufferReader::ReadCB> mock_read_cb;
  base::MockCallback<base::OnceClosure> mock_flush_cb;

  // Read callback should be fired after reset callback.
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_flush_cb, Run());
  EXPECT_CALL(mock_read_cb, Run(MatchesDecoderBuffer(media_buffer)))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // Write, reset, then read.
  mojom::DecoderBufferPtr mojo_buffer =
      converter.writer->WriteDecoderBuffer(media_buffer);
  converter.reader->Flush(mock_flush_cb.Get());
  converter.reader->ReadDecoderBuffer(std::move(mojo_buffer),
                                      mock_read_cb.Get());
  run_loop.Run();
}

TEST(MojoDecoderBufferConverterTest, FlushDuringChunkedRead) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  const uint8_t kData[] = "Lorem ipsum dolor sit amet, consectetur cras amet";
  const size_t kDataSize = base::size(kData);
  scoped_refptr<DecoderBuffer> media_buffer =
      DecoderBuffer::CopyFrom(kData, kDataSize);

  MojoDecoderBufferConverter converter(kDataSize / 3);

  base::MockCallback<MojoDecoderBufferReader::ReadCB> mock_read_cb;
  base::MockCallback<base::OnceClosure> mock_flush_cb;

  // Flush callback should be fired after read callback.
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_read_cb, Run(MatchesDecoderBuffer(media_buffer)));
  EXPECT_CALL(mock_flush_cb, Run())
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // Write, read, then reset.
  mojom::DecoderBufferPtr mojo_buffer =
      converter.writer->WriteDecoderBuffer(media_buffer);
  converter.reader->ReadDecoderBuffer(std::move(mojo_buffer),
                                      mock_read_cb.Get());
  converter.reader->Flush(mock_flush_cb.Get());
  run_loop.Run();
}

TEST(MojoDecoderBufferConverterTest, FlushDuringConcurrentReads) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  // Prevent all of the buffers from fitting at once to exercise the chunking
  // logic.
  MojoDecoderBufferConverter converter(4);
  auto& writer = converter.writer;
  auto& reader = converter.reader;

  // Three buffers: normal, EOS, normal.
  const uint8_t kData[] = "Hello, world";
  const size_t kDataSize = base::size(kData);
  auto media_buffer1 = DecoderBuffer::CopyFrom(kData, kDataSize);
  auto media_buffer2 = DecoderBuffer::CreateEOSBuffer();
  auto media_buffer3 = DecoderBuffer::CopyFrom(kData, kDataSize);

  // Expect the read callbacks to be issued in the same order.
  base::MockCallback<MojoDecoderBufferReader::ReadCB> mock_read_cb1;
  base::MockCallback<MojoDecoderBufferReader::ReadCB> mock_read_cb2;
  base::MockCallback<MojoDecoderBufferReader::ReadCB> mock_read_cb3;
  base::MockCallback<base::OnceClosure> mock_flush_cb;

  ::testing::InSequence scoper;
  EXPECT_CALL(mock_read_cb1, Run(MatchesDecoderBuffer(media_buffer1)));
  EXPECT_CALL(mock_read_cb2, Run(MatchesDecoderBuffer(media_buffer2)));
  EXPECT_CALL(mock_read_cb3, Run(MatchesDecoderBuffer(media_buffer3)));
  EXPECT_CALL(mock_flush_cb, Run())
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // Write all of the buffers at once.
  auto mojo_buffer1 = writer->WriteDecoderBuffer(media_buffer1);
  auto mojo_buffer2 = writer->WriteDecoderBuffer(media_buffer2);
  auto mojo_buffer3 = writer->WriteDecoderBuffer(media_buffer3);

  // Read all of the buffers at once.
  reader->ReadDecoderBuffer(std::move(mojo_buffer1), mock_read_cb1.Get());
  reader->ReadDecoderBuffer(std::move(mojo_buffer2), mock_read_cb2.Get());
  reader->ReadDecoderBuffer(std::move(mojo_buffer3), mock_read_cb3.Get());
  reader->Flush(mock_flush_cb.Get());
  // No ReadDecoderBuffer() can be called during pending reset.

  run_loop.Run();
}

}  // namespace media
