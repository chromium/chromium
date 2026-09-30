// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/test/test_future.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace content {
namespace {

using TraceDataEndpoint = TracingController::TraceDataEndpoint;

// Covers the bytes a text-mode FILE* would rewrite: '\n', "\r\n", a lone
// '\r', a NUL, and the Windows text-mode EOF character (0x1A).
std::string BinaryPayload() {
  std::string payload("header\ncrlf\r\ncr\r");
  payload.append(1, '\0');
  payload.append(1, '\x1a');
  // Every byte value, so any translation shows up as a mismatch.
  for (int i = 0; i <= 0xFF; ++i) {
    payload.append(1, static_cast<char>(i));
  }
  return payload;
}

class TracingControllerDataEndpointTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  base::FilePath TracePath() const {
    return temp_dir_.GetPath().AppendASCII("trace.out");
  }

  // A file endpoint writing to TracePath() and signalling |file_written_|.
  scoped_refptr<TraceDataEndpoint> CreateFileEndpoint() {
    return TracingController::CreateFileEndpoint(
        TracePath(), file_written_.GetCallback(),
        base::TaskPriority::USER_BLOCKING);
  }

  // Pushes |chunks| through |endpoint| and waits for the file to be written.
  void DrainThrough(scoped_refptr<TraceDataEndpoint> endpoint,
                    const std::vector<std::string>& chunks) {
    for (const std::string& chunk : chunks) {
      endpoint->ReceiveTraceChunk(std::make_unique<std::string>(chunk));
    }
    endpoint->ReceivedTraceFinalContents();
    EXPECT_TRUE(file_written_.Wait());
  }

  std::string ReadTraceFile() const {
    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(TracePath(), &contents));
    return contents;
  }

  BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::test::TestFuture<void> file_written_;
};

// The endpoint must not transform the bytes it is given.
TEST_F(TracingControllerDataEndpointTest, FileEndpointWritesBytesVerbatim) {
  const std::string payload = BinaryPayload();

  DrainThrough(CreateFileEndpoint(), {payload});

  EXPECT_EQ(payload, ReadTraceFile());
}

// The file must be the exact concatenation of the chunks, including across a
// boundary that splits a "\r\n" pair.
TEST_F(TracingControllerDataEndpointTest, FileEndpointConcatenatesChunks) {
  const std::vector<std::string> chunks = {"first\r", "\nsecond\n",
                                           std::string("\0third\n", 7)};
  std::string expected;
  for (const std::string& chunk : chunks) {
    expected += chunk;
  }

  DrainThrough(CreateFileEndpoint(), chunks);

  EXPECT_EQ(expected, ReadTraceFile());
}

// A session that produced no data still leaves an empty file behind.
TEST_F(TracingControllerDataEndpointTest, FileEndpointWithNoChunks) {
  DrainThrough(CreateFileEndpoint(), {});

  EXPECT_TRUE(base::PathExists(TracePath()));
  EXPECT_EQ(std::string(), ReadTraceFile());
}

// CompressedTraceDataEndpoint hands gzip data to the endpoint it wraps, so the
// file it writes must still decompress.
TEST_F(TracingControllerDataEndpointTest, CompressedFileEndpointRoundTrips) {
  const std::string payload = BinaryPayload();

  DrainThrough(TracingControllerImpl::CreateCompressedStringEndpoint(
                   CreateFileEndpoint(),
                   /*compress_with_background_priority=*/false),
               {payload});

  const std::string compressed = ReadTraceFile();
  ASSERT_FALSE(compressed.empty());
  std::string uncompressed;
  ASSERT_TRUE(compression::GzipUncompress(compressed, &uncompressed));
  EXPECT_EQ(payload, uncompressed);
}

// The string endpoint must preserve the bytes exactly too.
TEST_F(TracingControllerDataEndpointTest, StringEndpointPreservesBytes) {
  const std::string payload = BinaryPayload();

  base::test::TestFuture<std::unique_ptr<std::string>> received;
  auto endpoint =
      TracingController::CreateStringEndpoint(received.GetCallback());

  endpoint->ReceiveTraceChunk(std::make_unique<std::string>(payload));
  endpoint->ReceivedTraceFinalContents();

  EXPECT_EQ(payload, *received.Take());
}

}  // namespace
}  // namespace content
