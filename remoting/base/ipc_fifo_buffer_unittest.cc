// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/ipc_fifo_buffer.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "remoting/base/fifo_buffer_test_base.h"

namespace remoting {

namespace {
constexpr size_t kCapacity = 1024;
}  // namespace

class IpcFifoBufferTestDelegate {
 public:
  IpcFifoBufferTestDelegate() {
    CHECK(CreateIpcFifoBuffer(kCapacity, writer_, reader_));
  }

  FifoBufferWriter& GetWriter() { return *writer_; }
  FifoBufferReader& GetReader() { return *reader_; }

  void ResetWriter() { writer_.reset(); }
  void ResetReader() { reader_.reset(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<IpcFifoBufferWriter> writer_;
  std::unique_ptr<IpcFifoBufferReader> reader_;
};

using IpcFifoBufferTestTypes = testing::Types<IpcFifoBufferTestDelegate>;
INSTANTIATE_TYPED_TEST_SUITE_P(Ipc, FifoBufferTest, IpcFifoBufferTestTypes);

class IpcFifoBufferTest : public testing::Test {
 protected:
  IpcFifoBufferTestDelegate delegate_;
};

TEST_F(IpcFifoBufferTest, PeerClosed) {
  // This test is specific to IpcFifoBuffer's Mojo implementation.
  std::vector<uint8_t> data = {1, 2, 3, 4};
  EXPECT_EQ(delegate_.GetWriter().Write(data),
            FifoBufferWriter::Result::kSuccess);
  EXPECT_EQ(delegate_.GetReader().GetBufferedBytes(), 4u);

  // Destroy writer.
  delegate_.ResetWriter();

  // We should still be able to read what was already buffered.
  std::vector<uint8_t> read_data(4);
  EXPECT_EQ(delegate_.GetReader().Read(read_data), 4u);
  EXPECT_EQ(read_data, data);

  // Next read/query should fail gracefully (return nullopt) due to
  // FAILED_PRECONDITION.
  EXPECT_EQ(delegate_.GetReader().GetBufferedBytes(), std::nullopt);
  EXPECT_EQ(delegate_.GetReader().Read(read_data), std::nullopt);
  EXPECT_EQ(delegate_.GetReader().Skip(4), std::nullopt);
}

TEST_F(IpcFifoBufferTest, WriterClosed) {
  // This test is specific to IpcFifoBuffer's Mojo implementation.
  std::vector<uint8_t> data = {1, 2, 3, 4};
  delegate_.ResetReader();
  EXPECT_EQ(delegate_.GetWriter().Write(data),
            FifoBufferWriter::Result::kFailed);
}

}  // namespace remoting
