// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_writer.h"

#include <stddef.h>

#include <list>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

// A test implementation of ServiceWorkerCacheWriter::WriteObserver.
// This observer captures the response head or data sent to the observer
// for further checking.
class MockServiceWorkerCacheWriterObserver
    : public ServiceWorkerCacheWriter::WriteObserver {
 public:
  MockServiceWorkerCacheWriterObserver() : data_length_(0), result_(net::OK) {}

  MockServiceWorkerCacheWriterObserver(
      const MockServiceWorkerCacheWriterObserver&) = delete;
  MockServiceWorkerCacheWriterObserver& operator=(
      const MockServiceWorkerCacheWriterObserver&) = delete;

  ~MockServiceWorkerCacheWriterObserver() {}

  int WillWriteResponseHead(
      const network::mojom::URLResponseHead& response_head) override {
    response_ = response_head.Clone();
    return net::OK;
  }

  int WillWriteData(scoped_refptr<net::IOBuffer> data,
                    int length,
                    base::OnceCallback<void(net::Error)> callback) override {
    data_ = std::move(data);
    data_length_ = length;
    callback_ = std::move(callback);
    return result_;
  }

  // Call the |callback_| using |error| as input.
  void Complete(net::Error error) {
    if (callback_)
      std::move(callback_).Run(error);
  }

  // Set the return value of WillWriteData().
  void set_result(net::Error result) { result_ = result; }

  network::mojom::URLResponseHeadPtr response_;
  scoped_refptr<net::IOBuffer> data_;
  size_t data_length_;
  base::OnceCallback<void(net::Error)> callback_;
  net::Error result_;
};

class ServiceWorkerCacheWriterTest : public ::testing::Test {
 public:
  // Cache writer is created differently depending on diffrerent usage.
  enum class CacheWriterUsage {
    kForCopy,
    kForWriteBack,
    kForComparison,
  };

  ServiceWorkerCacheWriterTest() {}

  ServiceWorkerCacheWriterTest(const ServiceWorkerCacheWriterTest&) = delete;
  ServiceWorkerCacheWriterTest& operator=(const ServiceWorkerCacheWriterTest&) =
      delete;

  ~ServiceWorkerCacheWriterTest() override {}

  MockServiceWorkerResourceReader* ExpectReader() {
    auto reader = std::make_unique<MockServiceWorkerResourceReader>();
    MockServiceWorkerResourceReader* borrowed_reader = reader.get();
    readers_.push_back(std::move(reader));
    return borrowed_reader;
  }

  MockServiceWorkerResourceWriter* ExpectWriter() {
    auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
    MockServiceWorkerResourceWriter* borrowed_writer = writer.get();
    writers_.push_back(std::move(writer));
    return borrowed_writer;
  }

  // This should be called after ExpectReader() and ExpectWriter().
  void Initialize(CacheWriterUsage type, bool pause_when_not_identical) {
    switch (type) {
      case CacheWriterUsage::kForCopy:
        cache_writer_ = ServiceWorkerCacheWriter::CreateForCopy(
            CreateReader(), CreateWriter(),
            /*writer_resource_id=*/0);
        break;
      case CacheWriterUsage::kForWriteBack:
        cache_writer_ = ServiceWorkerCacheWriter::CreateForWriteBack(
            CreateWriter(), /*writer_resource_id=*/0);
        break;
      case CacheWriterUsage::kForComparison:
        auto compare_reader = CreateReader();
        auto copy_reader = CreateReader();
        cache_writer_ = ServiceWorkerCacheWriter::CreateForComparison(
            std::move(compare_reader), std::move(copy_reader), CreateWriter(),
            /*writer_resource_id=*/0, pause_when_not_identical,
            ServiceWorkerCacheWriter::ChecksumUpdateTiming::kCacheMismatch);
        break;
    };
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::list<std::unique_ptr<MockServiceWorkerResourceReader>> readers_;
  std::list<std::unique_ptr<MockServiceWorkerResourceWriter>> writers_;
  std::unique_ptr<ServiceWorkerCacheWriter> cache_writer_;
  bool write_complete_ = false;
  net::Error last_error_ = net::OK;

  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> CreateReader() {
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader> remote;
    if (readers_.empty())
      return remote;
    auto* reader_rawptr = readers_.front().get();
    remote.Bind(reader_rawptr->BindNewPipeAndPassRemote(
        // Keep the instance alive until the connection is destroyed.
        base::BindOnce([](std::unique_ptr<MockServiceWorkerResourceReader>) {},
                       std::move(readers_.front()))));
    readers_.pop_front();
    return remote;
  }

  mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> CreateWriter() {
    mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> remote;
    if (writers_.empty())
      return remote;
    auto* writer_rawptr = writers_.front().get();
    remote.Bind(writer_rawptr->BindNewPipeAndPassRemote(
        // Keep the instance alive until the connection is destroyed.
        base::BindOnce([](std::unique_ptr<MockServiceWorkerResourceWriter>) {},
                       std::move(writers_.front()))));
    writers_.pop_front();
    return remote;
  }

  ServiceWorkerCacheWriter::OnWriteCompleteCallback CreateWriteCallback() {
    return base::BindOnce(&ServiceWorkerCacheWriterTest::OnWriteComplete,
                          base::Unretained(this));
  }

  void OnWriteComplete(net::Error error) {
    write_complete_ = true;
    last_error_ = error;
  }

  net::Error WriteHeaders(size_t len) {
    auto response_head = network::mojom::URLResponseHead::New();
    const char data[] = "HTTP/1.1 200 OK\0\0";
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        std::string(data, std::size(data)));
    response_head->content_length = len;
    net::Error error = cache_writer_->MaybeWriteHeaders(
        std::move(response_head), CreateWriteCallback());
    return error;
  }

  net::Error WriteData(const std::string& data) {
    scoped_refptr<net::IOBuffer> buf =
        base::MakeRefCounted<net::StringIOBuffer>(data);
    net::Error error = cache_writer_->MaybeWriteData(buf.get(), data.size(),
                                                     CreateWriteCallback());
    base::RunLoop().RunUntilIdle();
    return error;
  }
};

// Passthrough tests:
// In these tests, the ServiceWorkerCacheWriter under test has no existing
// reader, since no calls to ExpectReader() have been made; this means that
// there is no existing cached response and the incoming data is written back to
// the cache directly.

TEST_F(ServiceWorkerCacheWriterTest, PassthroughHeadersAsync) {
  size_t kHeaderSize = 16;
  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  writer->ExpectWriteResponseHeadOk(kHeaderSize);
  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_FALSE(write_complete_);
  writer->CompletePendingWrite();
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(net::OK, last_error_);
  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_EQ(0U, cache_writer_->bytes_written());
}

TEST_F(ServiceWorkerCacheWriterTest, PassthroughDataAsync) {
  const std::string data1 = "abcdef";
  const std::string data2 = "ghijklmno";
  size_t response_size = data1.size() + data2.size();

  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  writer->ExpectWriteResponseHeadOk(response_size);
  writer->ExpectWriteDataOk(data1.size());
  writer->ExpectWriteDataOk(data2.size());
  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(response_size);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  writer->CompletePendingWrite();

  error = WriteData(data1);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  writer->CompletePendingWrite();
  EXPECT_TRUE(write_complete_);

  write_complete_ = false;
  error = WriteData(data2);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  writer->CompletePendingWrite();
  EXPECT_TRUE(write_complete_);
  // SHA256 hash for "abcdefghijklmno"
  EXPECT_EQ("41C7760C50EFDE99BF574ED8FFFC7A6DD3405D546D3DA929B214C8945ACF8A97",
            cache_writer_->GetSha256Checksum());

  EXPECT_EQ(net::OK, last_error_);
  EXPECT_TRUE(writer->AllExpectedWritesDone());
}

TEST_F(ServiceWorkerCacheWriterTest, PassthroughHeadersFailAsync) {
  size_t kHeaderSize = 16;
  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  writer->ExpectWriteResponseHead(kHeaderSize, net::ERR_FAILED);
  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_FALSE(write_complete_);
  writer->CompletePendingWrite();
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(net::ERR_FAILED, last_error_);
  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_EQ(0U, cache_writer_->bytes_written());
}

TEST_F(ServiceWorkerCacheWriterTest, PassthroughDataFailAsync) {
  const std::string data = "abcdef";

  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  writer->ExpectWriteResponseHeadOk(data.size());
  writer->ExpectWriteData(data.size(), net::ERR_FAILED);
  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);

  EXPECT_EQ(net::ERR_IO_PENDING, WriteHeaders(data.size()));
  writer->CompletePendingWrite();

  EXPECT_EQ(net::ERR_IO_PENDING, WriteData(data));
  writer->CompletePendingWrite();
  EXPECT_EQ(net::ERR_FAILED, last_error_);
  EXPECT_TRUE(write_complete_);
  EXPECT_TRUE(writer->AllExpectedWritesDone());
}

// Comparison tests:
// For the Compare* tests below, the ServiceWorkerCacheWriter under test has a
// reader for an existing cached response, so it will compare the response being
// written to it against the existing cached response.
TEST_F(ServiceWorkerCacheWriterTest, CompareDataOkAsync) {
  const std::string data1 = "abcdef";
  size_t response_size = data1.size();

  MockServiceWorkerResourceReader* reader = ExpectReader();

  // Create a copy reader and writer as they're needed to create cache writer
  // for comparison though not used in this test.
  ExpectReader();
  ExpectWriter();

  reader->ExpectReadResponseHeadOk(response_size);
  reader->ExpectReadDataOk(data1);
  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(response_size);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  reader->CompletePendingRead();

  error = WriteData(data1);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  reader->CompletePendingRead();

  EXPECT_TRUE(reader->AllExpectedReadsDone());
  EXPECT_EQ(0U, cache_writer_->bytes_written());
}

TEST_F(ServiceWorkerCacheWriterTest, CompareDataManyOkAsync) {
  const std::string expected_data[] = {
      "abcdef",
      "ghijkl",
      "mnopqr",
      "stuvwxyz",
  };
  size_t response_size = 0;
  for (const auto& chunk : expected_data)
    response_size += chunk.size();

  MockServiceWorkerResourceReader* reader = ExpectReader();

  // Create a copy reader and writer as they're needed to create cache writer
  // for comparison though not used in this test.
  ExpectReader();
  ExpectWriter();

  reader->ExpectReadResponseHeadOk(response_size);
  for (const auto& chunk : expected_data) {
    reader->ExpectReadDataOk(chunk);
  }
  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(response_size);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  reader->CompletePendingRead();

  for (const auto& chunk : expected_data) {
    error = WriteData(chunk);
    EXPECT_EQ(net::ERR_IO_PENDING, error);
    reader->CompletePendingRead();
    EXPECT_EQ(net::OK, last_error_);
  }

  EXPECT_TRUE(reader->AllExpectedReadsDone());
  EXPECT_EQ(0U, cache_writer_->bytes_written());
}

// This test writes headers and three data blocks data1, data2, data3; data2
// differs in the cached version. The writer should be asked to rewrite the
// headers and body with the new value, and the copy reader should be asked to
// read the header and data1.
TEST_F(ServiceWorkerCacheWriterTest, CompareFailedCopy) {
  std::string data1 = "abcdef";
  std::string cache_data2 = "ghijkl";
  std::string net_data2 = "mnopqr";
  std::string data3 = "stuvwxyz";
  size_t cache_response_size = data1.size() + cache_data2.size() + data3.size();
  size_t net_response_size = data1.size() + net_data2.size() + data3.size();

  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  MockServiceWorkerResourceReader* compare_reader = ExpectReader();
  MockServiceWorkerResourceReader* copy_reader = ExpectReader();

  compare_reader->ExpectReadResponseHeadOk(cache_response_size);
  compare_reader->ExpectReadDataOk(data1);
  compare_reader->ExpectReadDataOk(cache_data2);

  copy_reader->ExpectReadResponseHeadOk(cache_response_size);
  copy_reader->ExpectReadDataOk(data1);

  writer->ExpectWriteResponseHeadOk(net_response_size);
  writer->ExpectWriteDataOk(data1.size());
  writer->ExpectWriteDataOk(net_data2.size());
  writer->ExpectWriteDataOk(data3.size());

  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(net_response_size);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  compare_reader->CompletePendingRead();
  EXPECT_EQ(net::OK, last_error_);

  error = WriteData(data1);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  compare_reader->CompletePendingRead();
  EXPECT_EQ(net::OK, last_error_);

  error = WriteData(net_data2);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  compare_reader->CompletePendingRead();

  // At this point, |copy_reader| is asked to read the header and data1.
  copy_reader->CompletePendingRead();
  writer->CompletePendingWrite();
  copy_reader->CompletePendingRead();
  EXPECT_EQ(net::OK, last_error_);
  writer->CompletePendingWrite();
  // Complete a write of |net_data2| to the |writer|.
  writer->CompletePendingWrite();

  // |data3| goes directly to the response writer.
  error = WriteData(data3);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  writer->CompletePendingWrite();

  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_TRUE(compare_reader->AllExpectedReadsDone());
  EXPECT_TRUE(copy_reader->AllExpectedReadsDone());
}

// Tests behavior when the cached data is shorter than the network data.
TEST_F(ServiceWorkerCacheWriterTest, CompareFailedCopyShort) {
  std::string data1 = "abcdef";
  std::string cache_data2 = "mnop";
  std::string net_data2 = "mnopqr";
  std::string data3 = "stuvwxyz";
  size_t cache_response_size = data1.size() + cache_data2.size() + data3.size();
  size_t net_response_size = data1.size() + net_data2.size() + data3.size();

  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  MockServiceWorkerResourceReader* compare_reader = ExpectReader();
  MockServiceWorkerResourceReader* copy_reader = ExpectReader();

  compare_reader->ExpectReadResponseHeadOk(cache_response_size);
  compare_reader->ExpectReadDataOk(data1);
  compare_reader->ExpectReadDataOk(cache_data2);
  compare_reader->ExpectReadDataOk("");  // EOF read

  copy_reader->ExpectReadResponseHeadOk(cache_response_size);
  copy_reader->ExpectReadDataOk(data1);

  writer->ExpectWriteResponseHeadOk(net_response_size);
  writer->ExpectWriteDataOk(data1.size());
  writer->ExpectWriteDataOk(net_data2.size());
  writer->ExpectWriteDataOk(data3.size());

  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(net_response_size);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  // Read the header from |compare_reader|.
  compare_reader->CompletePendingRead();
  EXPECT_EQ(net::OK, last_error_);

  error = WriteData(data1);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  // Read |data1| from |compare_reader| for the comparison.
  compare_reader->CompletePendingRead();
  EXPECT_EQ(net::OK, last_error_);

  error = WriteData(net_data2);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  // Read |cache_data2| and |data3| from |compare_reader|.
  compare_reader->CompletePendingRead();
  compare_reader->CompletePendingRead();
  // After that, the cache writer uses |copy_reader| to read the header and
  // |data1|.
  copy_reader->CompletePendingRead();
  writer->CompletePendingWrite();
  copy_reader->CompletePendingRead();
  writer->CompletePendingWrite();
  EXPECT_EQ(net::OK, last_error_);

  // |net_data2| is written to the |writer|.
  writer->CompletePendingWrite();
  error = WriteData(data3);
  // |data3| is directly written to the disk.
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  writer->CompletePendingWrite();

  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_TRUE(compare_reader->AllExpectedReadsDone());
  EXPECT_TRUE(copy_reader->AllExpectedReadsDone());
}

// Tests behavior when the cached data is longer than the network data.
TEST_F(ServiceWorkerCacheWriterTest, CompareFailedCopyLong) {
  std::string data1 = "abcdef";
  std::string cache_data2 = "mnop";
  std::string net_data2 = "mnop";
  std::string cache_data3 = "qr";
  size_t cached_size = data1.size() + cache_data2.size() + cache_data3.size();
  size_t net_size = data1.size() + net_data2.size();

  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  MockServiceWorkerResourceReader* compare_reader = ExpectReader();
  MockServiceWorkerResourceReader* copy_reader = ExpectReader();

  compare_reader->ExpectReadResponseHeadOk(cached_size);
  compare_reader->ExpectReadDataOk(data1);
  compare_reader->ExpectReadDataOk(cache_data2);

  // The comparison should fail at the end of |cache_data2|, when the cache
  // writer realizes the two responses are different sizes, and then the network
  // data should be written back starting with |net_data2|.
  copy_reader->ExpectReadResponseHeadOk(cached_size);
  copy_reader->ExpectReadDataOk(data1);
  copy_reader->ExpectReadDataOk(net_data2);

  writer->ExpectWriteResponseHeadOk(net_size);
  writer->ExpectWriteDataOk(data1.size());
  writer->ExpectWriteDataOk(net_data2.size());

  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(net_size);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  // Read the header from |compare_reader| for the comparison.
  compare_reader->CompletePendingRead();
  EXPECT_EQ(net::OK, last_error_);

  error = WriteData(data1);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  // Read |data1| from |compare_reader| for the comparison.
  compare_reader->CompletePendingRead();
  EXPECT_EQ(net::OK, last_error_);

  error = WriteData(net_data2);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  // Read |cache_data2| from |compare_reader| for the comparison.
  compare_reader->CompletePendingRead();
  EXPECT_EQ(net::OK, last_error_);

  error = WriteData("");
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  // Diff is found and copying starts.
  // Read the header from |copy_reader|.
  copy_reader->CompletePendingRead();
  writer->CompletePendingWrite();
  // Read |data1| from |copy_reader| to copy.
  copy_reader->CompletePendingRead();
  writer->CompletePendingWrite();
  // Read |net_data_2| from |copy_reader|.
  copy_reader->CompletePendingRead();
  writer->CompletePendingWrite();
  EXPECT_EQ(net::OK, last_error_);

  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_TRUE(compare_reader->AllExpectedReadsDone());
  EXPECT_TRUE(copy_reader->AllExpectedReadsDone());
}

// Tests behavior when the compare reader does not complete in single try and
// needs to issue another read.
TEST_F(ServiceWorkerCacheWriterTest, MultipleComparisonInSingleWrite) {
  // Data for |compare_reader|.
  const std::vector<std::string> data_from_cache{"a", "b", "c"};

  // Data for |writer|. The first 2 bytes are provided in a larger chunk than
  // the |compare_reader| does.
  const std::vector<std::string> data_from_net{"ab", "x"};

  // Data for |copy_reader|. The comparison between cache and network data fails
  // at the 3rd byte, so the cache writer will read only first 2 bytes from the
  // |copy_reader|.
  const std::vector<std::string> data_to_copy{"ab"};

  // The written data is expected to be identical with |data_from_net|.
  const std::vector<std::string> data_expected{"ab", "x"};

  size_t bytes_cached = 0;
  size_t bytes_from_net = 0;
  size_t bytes_common = 0;

  for (const auto& data : data_from_cache)
    bytes_cached += data.size();

  for (const auto& data : data_from_net)
    bytes_from_net += data.size();

  for (const auto& data : data_to_copy)
    bytes_common += data.size();

  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  MockServiceWorkerResourceReader* compare_reader = ExpectReader();
  MockServiceWorkerResourceReader* copy_reader = ExpectReader();

  compare_reader->ExpectReadResponseHeadOk(bytes_cached);
  for (const auto& data : data_from_cache)
    compare_reader->ExpectReadDataOk(data);

  copy_reader->ExpectReadResponseHeadOk(bytes_common);
  for (const auto& data : data_to_copy)
    copy_reader->ExpectReadDataOk(data);

  writer->ExpectWriteResponseHeadOk(bytes_from_net);
  for (const auto& data : data_expected)
    writer->ExpectWriteDataOk(data.size());

  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(bytes_from_net);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  // Read the header from |compare_reader| for the comparison.
  compare_reader->CompletePendingRead();
  EXPECT_EQ(net::OK, last_error_);

  for (const auto& data : data_from_net) {
    error = WriteData(data);
    EXPECT_EQ(net::ERR_IO_PENDING, error);
    for (size_t i = 0; i < data.size(); ++i) {
      // Read the body from |compare_reader|. Repeat data.size() times because
      // each chunk in |data_from_cache| is 1 byte.
      compare_reader->CompletePendingRead();
      EXPECT_EQ(net::OK, last_error_);
    }
  }

  // At the end of the chunk, there's a diff so the header and a chunk of body
  // is read from |copy_reader|. Read the header from |compare_reader|.
  copy_reader->CompletePendingRead();
  EXPECT_EQ(net::OK, last_error_);
  writer->CompletePendingWrite();
  EXPECT_EQ(net::OK, last_error_);

  // Read the first chunk from |compare_reader|.
  copy_reader->CompletePendingRead();
  EXPECT_EQ(net::OK, last_error_);
  writer->CompletePendingWrite();
  EXPECT_EQ(net::OK, last_error_);
  // |data_from_net| is written to the |writer|.
  writer->CompletePendingWrite();
  EXPECT_EQ(net::OK, last_error_);

  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_TRUE(compare_reader->AllExpectedReadsDone());
  EXPECT_TRUE(copy_reader->AllExpectedReadsDone());
}

// Tests behavior when |pause_when_not_identical| is enabled and cache writer
// finishes asynchronously.
TEST_F(ServiceWorkerCacheWriterTest, PauseWhenNotIdentical_AsyncWriteData) {
  // Data from |compare_reader|.
  const std::vector<std::string> data_from_cache{"abcd"};

  // Data for |writer|. The comparison should stop at the first block of the
  // data.
  const std::vector<std::string> data_from_net{"abxx"};

  // We don't need |data_to_copy| because the network data and the cached data
  // have no common blocks.

  // The written data should be the same as |data_from_net|.
  const std::vector<std::string> data_expected{"abxx"};

  size_t bytes_cached = 0;
  size_t bytes_from_net = 0;
  size_t bytes_expected = 0;

  for (const auto& data : data_from_cache)
    bytes_cached += data.size();

  for (const auto& data : data_from_net)
    bytes_from_net += data.size();

  for (const auto& data : data_expected)
    bytes_expected += data.size();

  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  MockServiceWorkerResourceReader* compare_reader = ExpectReader();
  MockServiceWorkerResourceReader* copy_reader = ExpectReader();

  compare_reader->ExpectReadResponseHeadOk(bytes_cached);
  for (const auto& data : data_from_cache)
    compare_reader->ExpectReadDataOk(data);

  copy_reader->ExpectReadResponseHeadOk(bytes_cached);

  writer->ExpectWriteResponseHeadOk(bytes_expected);
  for (const auto& data : data_expected)
    writer->ExpectWriteDataOk(data.size());

  Initialize(CacheWriterUsage::kForComparison,
             true /* pause_when_not_identical */);

  write_complete_ = false;
  net::Error error = WriteHeaders(bytes_from_net);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_FALSE(write_complete_);
  compare_reader->CompletePendingRead();
  EXPECT_TRUE(write_complete_);

  // The comparison is suspended due to an asynchronous read of
  // |compare_reader|, resulting in an early return. At this point, the callback
  // shouldn't be called yet.
  write_complete_ = false;
  error = WriteData(data_from_net[0]);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_FALSE(write_complete_);

  // When |compare_reader| succeeds in reading the stored data, |cache_writer_|
  // then proceeds to the comparison phase.
  // |cache_writer_| stops comparison at the first block of the data.
  // Since |pause_when_not_identical| is enabled, it should subsequently trigger
  // the callback and return net::ERR_IO_PENDING.
  compare_reader->CompletePendingRead();
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(net::ERR_IO_PENDING, last_error_);
  EXPECT_EQ(0U, cache_writer_->bytes_written());

  // Resume |cache_writer_| with a callback which updates |write_complete_| and
  // |last_error_| when it's called.
  // |copy_reader| does an asynchronous read here.
  write_complete_ = false;
  error = cache_writer_->Resume(CreateWriteCallback());
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_FALSE(write_complete_);

  // Complete the asynchronous read of the header. Since there's nothing to copy
  // from the storage, |copy_reader| should finish all its jobs here.
  copy_reader->CompletePendingRead();
  EXPECT_TRUE(copy_reader->AllExpectedReadsDone());

  // Complete the asynchronous write of the header. This doesn't finish all the
  // write to the storage, so the callback isn't called yet.
  writer->CompletePendingWrite();
  EXPECT_FALSE(write_complete_);
  EXPECT_EQ(net::ERR_IO_PENDING, last_error_);

  // Complete the asynchronous write of the body. This completes all the work of
  // |cache_writer|, so the callback is triggered.
  writer->CompletePendingWrite();
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(net::OK, last_error_);
  EXPECT_EQ(bytes_expected, cache_writer_->bytes_written());

  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_TRUE(compare_reader->AllExpectedReadsDone());
}

// Tests behavior of a cache writer used to copy script which finishes
// asynchronously.
TEST_F(ServiceWorkerCacheWriterTest, CopyScript_Async) {
  // Data from |copy_reader|.
  const std::vector<std::string> data_from_cache{"abcd"};

  // The written data should be the same as |data_from_cache|.
  const std::vector<std::string> data_expected{"abcd"};

  size_t bytes_cached = 0;
  size_t bytes_expected = 0;

  for (const auto& data : data_from_cache)
    bytes_cached += data.size();

  for (const auto& data : data_expected)
    bytes_expected += data.size();

  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  MockServiceWorkerResourceReader* copy_reader = ExpectReader();

  copy_reader->ExpectReadResponseHeadOk(bytes_cached);
  for (const auto& data : data_from_cache)
    copy_reader->ExpectReadDataOk(data);

  writer->ExpectWriteResponseHeadOk(bytes_expected);
  for (const auto& data : data_expected)
    writer->ExpectWriteDataOk(data.size());

  Initialize(CacheWriterUsage::kForCopy, false /* pause_when_not_identical */);

  write_complete_ = false;
  net::Error error = cache_writer_->StartCopy(CreateWriteCallback());
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_FALSE(write_complete_);

  // Complete the asynchronous read of the header. This doesn't finish all the
  // read to the storage, so the callback isn't called yet.
  copy_reader->CompletePendingRead();
  EXPECT_FALSE(write_complete_);

  // Complete the asynchronous write of the header. This doesn't finish all the
  // write to the storage, so the callback isn't called yet.
  writer->CompletePendingWrite();
  EXPECT_FALSE(write_complete_);

  // Complete the asynchronous read of the data. This finishes all the
  // read to the storage. But the write has not ben performed, so the
  // callback isn't called yet.
  copy_reader->CompletePendingRead();
  EXPECT_FALSE(write_complete_);
  EXPECT_TRUE(copy_reader->AllExpectedReadsDone());

  // Complete the asynchronous write of the data. This finishes all the
  // write to the storage, so the callback is called.
  writer->CompletePendingWrite();
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(net::OK, last_error_);
  EXPECT_EQ(bytes_expected, cache_writer_->bytes_written());
  EXPECT_TRUE(writer->AllExpectedWritesDone());
}

// Tests behavior of a cache writer used to copy script that read multiple
// times and finishes asynchronously.
TEST_F(ServiceWorkerCacheWriterTest, CopyScript_AsyncMultipleRead) {
  // Data from |copy_reader|.
  const std::vector<std::string> data_from_cache{"a", "bc", "d"};

  // The written data should be the same as |data_from_cache|.
  const std::vector<std::string> data_expected{"a", "bc", "d"};

  size_t bytes_cached = 0;
  size_t bytes_expected = 0;

  for (const auto& data : data_from_cache)
    bytes_cached += data.size();

  for (const auto& data : data_expected)
    bytes_expected += data.size();

  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  MockServiceWorkerResourceReader* copy_reader = ExpectReader();

  copy_reader->ExpectReadResponseHeadOk(bytes_cached);
  for (const auto& data : data_from_cache)
    copy_reader->ExpectReadDataOk(data);

  writer->ExpectWriteResponseHeadOk(bytes_expected);
  for (const auto& data : data_expected)
    writer->ExpectWriteDataOk(data.size());

  Initialize(CacheWriterUsage::kForCopy, false /* pause_when_not_identical */);

  write_complete_ = false;
  net::Error error = cache_writer_->StartCopy(CreateWriteCallback());
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_FALSE(write_complete_);

  // Complete the asynchronous read of the header. This doesn't finish all the
  // read to the storage, so the callback isn't called yet.
  copy_reader->CompletePendingRead();
  EXPECT_FALSE(write_complete_);

  // Complete the asynchronous write of the header. This doesn't finish all the
  // write to the storage, so the callback isn't called yet.
  writer->CompletePendingWrite();
  EXPECT_FALSE(write_complete_);

  // Complete the asynchronous read of data block "a". This doesn't finish all
  // the read to the storage, so the callback isn't called yet.
  copy_reader->CompletePendingRead();
  EXPECT_FALSE(write_complete_);

  // Complete the asynchronous write of data block "a". This doesn't finish all
  // the write to the storage, so the callback isn't called yet.
  writer->CompletePendingWrite();
  EXPECT_FALSE(write_complete_);

  // Complete the asynchronous read of data block "bc". This doesn't finish all
  // the read to the storage, so the callback isn't called yet.
  copy_reader->CompletePendingRead();
  EXPECT_FALSE(write_complete_);

  // Complete the asynchronous write of the data block "bc". This doesn't finish
  // all the write to the storage, so the callback isn't called yet.
  writer->CompletePendingWrite();
  EXPECT_FALSE(write_complete_);

  // Complete the asynchronous read of data block "d". This finishes all the
  // read to the storage. But the write has not ben performed, so the
  // callback isn't called yet.
  copy_reader->CompletePendingRead();
  EXPECT_FALSE(write_complete_);
  EXPECT_TRUE(copy_reader->AllExpectedReadsDone());

  // Complete the asynchronous write of data block "d". This finishes all the
  // write to the storage, so the callback is called.
  writer->CompletePendingWrite();
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(net::OK, last_error_);
  EXPECT_EQ(bytes_expected, cache_writer_->bytes_written());
  EXPECT_TRUE(writer->AllExpectedWritesDone());
}

// The observer runs synchronously and the response writer runs asynchronously.
TEST_F(ServiceWorkerCacheWriterTest, ObserverSyncResponseWriterAsync) {
  const size_t kHeaderSize = 16;
  const std::string data = "abcdef";
  size_t response_size = data.size();

  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  writer->ExpectWriteResponseHeadOk(kHeaderSize);
  writer->ExpectWriteDataOk(data.size());

  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);
  MockServiceWorkerCacheWriterObserver observer;
  cache_writer_->set_write_observer(&observer);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_TRUE(observer.response_);
  writer->CompletePendingWrite();
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(last_error_, net::OK);

  error = WriteData(data);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_EQ(observer.data_length_, response_size);
  EXPECT_TRUE(observer.data_);
  writer->CompletePendingWrite();
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(last_error_, net::OK);

  cache_writer_->set_write_observer(nullptr);

  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_EQ(response_size, cache_writer_->bytes_written());
}

// The observer and response writer all run asynchronously.
TEST_F(ServiceWorkerCacheWriterTest, ObserverAsyncResponseWriterAsync) {
  const size_t kHeaderSize = 16;
  const std::string data = "abcdef";
  size_t response_size = data.size();

  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  writer->ExpectWriteResponseHeadOk(kHeaderSize);
  writer->ExpectWriteDataOk(data.size());

  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);
  MockServiceWorkerCacheWriterObserver observer;
  cache_writer_->set_write_observer(&observer);
  observer.set_result(net::ERR_IO_PENDING);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_TRUE(observer.response_);
  writer->CompletePendingWrite();
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(last_error_, net::OK);

  error = WriteData(data);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  observer.Complete(net::OK);
  EXPECT_EQ(observer.data_length_, response_size);
  EXPECT_TRUE(observer.data_);
  writer->CompletePendingWrite();
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(last_error_, net::OK);

  cache_writer_->set_write_observer(nullptr);

  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_EQ(response_size, cache_writer_->bytes_written());
}

// Observer's OnWillWriteData() runs synchronously but fails.
TEST_F(ServiceWorkerCacheWriterTest, ObserverSyncFail) {
  const size_t kHeaderSize = 16;
  const std::string data = "abcdef";

  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  writer->ExpectWriteResponseHeadOk(kHeaderSize);
  writer->ExpectWriteDataOk(data.size());

  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);
  MockServiceWorkerCacheWriterObserver observer;
  cache_writer_->set_write_observer(&observer);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_TRUE(observer.response_);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  writer->CompletePendingWrite();

  observer.set_result(net::ERR_FAILED);
  error = WriteData(data);
  EXPECT_EQ(net::ERR_FAILED, error);
  EXPECT_EQ(0U, cache_writer_->bytes_written());

  cache_writer_->set_write_observer(nullptr);
}

// Observer's OnWillWriteData() runs asynchronously but fails.
TEST_F(ServiceWorkerCacheWriterTest, ObserverAsyncFail) {
  const size_t kHeaderSize = 16;
  const std::string data = "abcdef";

  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  writer->ExpectWriteResponseHeadOk(kHeaderSize);
  writer->ExpectWriteDataOk(data.size());

  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);
  MockServiceWorkerCacheWriterObserver observer;
  cache_writer_->set_write_observer(&observer);
  observer.set_result(net::ERR_IO_PENDING);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_TRUE(observer.response_);
  writer->CompletePendingWrite();

  error = WriteData(data);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  observer.Complete(net::ERR_FAILED);
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(last_error_, net::ERR_FAILED);
  EXPECT_EQ(0U, cache_writer_->bytes_written());

  cache_writer_->set_write_observer(nullptr);
}

class ServiceWorkerCacheWriterSha256ChecksumTest
    : public ServiceWorkerCacheWriterTest,
      public testing::WithParamInterface<
          ServiceWorkerCacheWriter::ChecksumUpdateTiming> {
 public:
  void Initialize() {
    auto compare_reader = CreateReader();
    auto copy_reader = CreateReader();
    cache_writer_ = ServiceWorkerCacheWriter::CreateForComparison(
        std::move(compare_reader), std::move(copy_reader), CreateWriter(),
        /*writer_resource_id=*/0, /*pause_when_not_identical=*/false,
        GetChecksumUpdateTiming());
  }

 protected:
  ServiceWorkerCacheWriter::ChecksumUpdateTiming GetChecksumUpdateTiming() {
    return GetParam();
  }
};

TEST_P(ServiceWorkerCacheWriterSha256ChecksumTest, CompareDataOk) {
  const std::string data = "abcdef";
  size_t response_size = data.size();

  MockServiceWorkerResourceReader* reader = ExpectReader();

  // Create a copy reader and writer as they're needed to create cache writer
  // for comparison though not used in this test.
  ExpectReader();
  ExpectWriter();

  reader->ExpectReadResponseHeadOk(response_size);
  reader->ExpectReadDataOk(data);
  Initialize();

  net::Error error = WriteHeaders(response_size);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  reader->CompletePendingRead();

  error = WriteData(data);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  reader->CompletePendingRead();

  EXPECT_TRUE(reader->AllExpectedReadsDone());

  std::string expected_checksum;
  switch (GetChecksumUpdateTiming()) {
    case ServiceWorkerCacheWriter::ChecksumUpdateTiming::kAlways:
      // Expected value is calculated from SHA256("abcdef")
      expected_checksum =
          "BEF57EC7F53A6D40BEB640A780A639C83BC29AC8A9816F1FC6C5C6DCD93C4721";
      break;
    case ServiceWorkerCacheWriter::ChecksumUpdateTiming::kCacheMismatch:
      // Expected value is calculated from SHA256("")
      expected_checksum =
          "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855";
      break;
  }
  EXPECT_EQ(expected_checksum, cache_writer_->GetSha256Checksum());
}

TEST_P(ServiceWorkerCacheWriterSha256ChecksumTest, CompareFailed) {
  std::string data1 = "abcdef";
  std::string cache_data2 = "mnop";
  std::string net_data2 = "mnopqr";
  std::string data3 = "stuvwxyz";
  size_t cache_response_size = data1.size() + cache_data2.size() + data3.size();
  size_t net_response_size = data1.size() + net_data2.size() + data3.size();

  MockServiceWorkerResourceWriter* writer = ExpectWriter();
  MockServiceWorkerResourceReader* compare_reader = ExpectReader();
  MockServiceWorkerResourceReader* copy_reader = ExpectReader();

  compare_reader->ExpectReadResponseHeadOk(cache_response_size);
  compare_reader->ExpectReadDataOk(data1);
  compare_reader->ExpectReadDataOk(cache_data2);
  compare_reader->ExpectReadDataOk("");  // EOF read

  copy_reader->ExpectReadResponseHeadOk(cache_response_size);
  copy_reader->ExpectReadDataOk(data1);

  writer->ExpectWriteResponseHeadOk(net_response_size);
  writer->ExpectWriteDataOk(data1.size());
  writer->ExpectWriteDataOk(net_data2.size());
  writer->ExpectWriteDataOk(data3.size());

  Initialize();

  net::Error error = WriteHeaders(net_response_size);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  // Read the header from |compare_reader|.
  compare_reader->CompletePendingRead();
  EXPECT_EQ(net::OK, last_error_);

  error = WriteData(data1);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  // Read |data1| from |compare_reader| for the comparison.
  compare_reader->CompletePendingRead();
  EXPECT_EQ(net::OK, last_error_);

  error = WriteData(net_data2);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  // Read |cache_data2| and |data3| from |compare_reader|.
  compare_reader->CompletePendingRead();
  compare_reader->CompletePendingRead();
  // After that, the cache writer uses |copy_reader| to read the header and
  // |data1|.
  copy_reader->CompletePendingRead();
  writer->CompletePendingWrite();
  copy_reader->CompletePendingRead();
  writer->CompletePendingWrite();
  EXPECT_EQ(net::OK, last_error_);

  // |net_data2| is written to the |writer|.
  writer->CompletePendingWrite();
  // |data3| is directly written to the disk.
  error = WriteData(data3);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  writer->CompletePendingWrite();

  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_TRUE(compare_reader->AllExpectedReadsDone());
  EXPECT_TRUE(copy_reader->AllExpectedReadsDone());

  // Expected value is calculated from SHA256("abcdefmnopqrstuvwxyz")
  EXPECT_EQ("50DCEABE70B3474ACF0E608D9E77B1ED2700FB74431FA8D8E0ED62ECDA7DCFEB",
            cache_writer_->GetSha256Checksum());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ServiceWorkerCacheWriterSha256ChecksumTest,
    testing::Values(
        ServiceWorkerCacheWriter::ChecksumUpdateTiming::kCacheMismatch,
        ServiceWorkerCacheWriter::ChecksumUpdateTiming::kAlways));

class ServiceWorkerCacheWriterDisconnectionTest
    : public ServiceWorkerCacheWriterTest {
 public:
  ServiceWorkerCacheWriterDisconnectionTest() = default;
  ~ServiceWorkerCacheWriterDisconnectionTest() override = default;

  void InitializeForWriteBack() {
    writer_ = std::make_unique<MockServiceWorkerResourceWriter>();
    mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> remote_writer;
    remote_writer.Bind(writer_->BindNewPipeAndPassRemote(base::DoNothing()));

    cache_writer_ = ServiceWorkerCacheWriter::CreateForWriteBack(
        std::move(remote_writer), /*writer_resource_id=*/0);
  }

  void InitializeForCopy() {
    writer_ = std::make_unique<MockServiceWorkerResourceWriter>();
    mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> remote_writer;
    remote_writer.Bind(writer_->BindNewPipeAndPassRemote(base::DoNothing()));

    copy_reader_ = std::make_unique<MockServiceWorkerResourceReader>();
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader>
        remote_copy_reader;
    remote_copy_reader.Bind(
        copy_reader_->BindNewPipeAndPassRemote(base::DoNothing()));

    cache_writer_ = ServiceWorkerCacheWriter::CreateForCopy(
        std::move(remote_copy_reader), std::move(remote_writer),
        /*writer_resource_id=*/0);
  }

  void InitializeForComparison(bool pause_when_not_identical) {
    writer_ = std::make_unique<MockServiceWorkerResourceWriter>();
    mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> remote_writer;
    remote_writer.Bind(writer_->BindNewPipeAndPassRemote(base::DoNothing()));

    copy_reader_ = std::make_unique<MockServiceWorkerResourceReader>();
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader>
        remote_copy_reader;
    remote_copy_reader.Bind(
        copy_reader_->BindNewPipeAndPassRemote(base::DoNothing()));

    compare_reader_ = std::make_unique<MockServiceWorkerResourceReader>();
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader>
        remote_compare_reader;
    remote_compare_reader.Bind(
        compare_reader_->BindNewPipeAndPassRemote(base::DoNothing()));

    cache_writer_ = ServiceWorkerCacheWriter::CreateForComparison(
        std::move(remote_compare_reader), std::move(remote_copy_reader),
        std::move(remote_writer),
        /*writer_resource_id=*/0, pause_when_not_identical,
        ServiceWorkerCacheWriter::ChecksumUpdateTiming::kCacheMismatch);
  }

  void SimulateDisconnection() {
    // Destroy readers and the writer to disconnect remotes in `cache_writer_`.
    writer_.reset();
    copy_reader_.reset();
    compare_reader_.reset();
    cache_writer_->FlushRemotesForTesting();
  }

 protected:
  std::unique_ptr<MockServiceWorkerResourceWriter> writer_;
  std::unique_ptr<MockServiceWorkerResourceReader> copy_reader_;
  std::unique_ptr<MockServiceWorkerResourceReader> compare_reader_;
};

TEST_F(ServiceWorkerCacheWriterDisconnectionTest, WriteBackBeforeHeader) {
  size_t kHeaderSize = 16;

  InitializeForWriteBack();
  writer_->ExpectWriteResponseHeadOk(kHeaderSize);

  SimulateDisconnection();

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_EQ(error, net::ERR_FAILED);
}

TEST_F(ServiceWorkerCacheWriterDisconnectionTest, WriteBackBeforeData) {
  const std::string data1 = "abcdef";
  const size_t response_size = data1.size();

  InitializeForWriteBack();
  writer_->ExpectWriteResponseHeadOk(response_size);
  writer_->ExpectWriteDataOk(data1.size());

  net::Error error = WriteHeaders(response_size);
  EXPECT_EQ(error, net::ERR_IO_PENDING);
  writer_->CompletePendingWrite();
  EXPECT_EQ(last_error_, net::OK);
  EXPECT_TRUE(write_complete_);
  write_complete_ = false;

  SimulateDisconnection();

  error = WriteData(data1);
  EXPECT_EQ(error, net::ERR_FAILED);
}

TEST_F(ServiceWorkerCacheWriterDisconnectionTest, WriteBackDuringData) {
  const std::string data1 = "abcdef";
  const std::string data2 = "ghijklmno";
  const size_t response_size = data1.size() + data2.size();

  InitializeForWriteBack();
  writer_->ExpectWriteResponseHeadOk(response_size);
  writer_->ExpectWriteDataOk(data1.size());
  writer_->ExpectWriteDataOk(data2.size());

  net::Error error = WriteHeaders(response_size);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  writer_->CompletePendingWrite();
  EXPECT_EQ(last_error_, net::OK);
  EXPECT_TRUE(write_complete_);
  write_complete_ = false;

  error = WriteData(data1);
  EXPECT_EQ(error, net::ERR_IO_PENDING);
  writer_->CompletePendingWrite();
  EXPECT_EQ(last_error_, net::OK);
  EXPECT_TRUE(write_complete_);
  write_complete_ = false;

  error = WriteData(data2);
  EXPECT_EQ(error, net::ERR_IO_PENDING);

  SimulateDisconnection();

  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(last_error_, net::ERR_FAILED);
}

TEST_F(ServiceWorkerCacheWriterDisconnectionTest, CopyBeforeStart) {
  const std::string data1 = "abcd";
  const size_t response_size = data1.size();

  InitializeForCopy();
  writer_->ExpectWriteResponseHeadOk(response_size);
  writer_->ExpectWriteDataOk(data1.size());

  SimulateDisconnection();

  net::Error error = cache_writer_->StartCopy(CreateWriteCallback());
  EXPECT_EQ(error, net::ERR_FAILED);
}

TEST_F(ServiceWorkerCacheWriterDisconnectionTest, CopyBeforeHeaderRead) {
  const std::string data1 = "abcd";
  const size_t response_size = data1.size();

  InitializeForCopy();
  copy_reader_->ExpectReadResponseHeadOk(response_size);
  copy_reader_->ExpectReadDataOk(data1);
  writer_->ExpectWriteResponseHeadOk(response_size);
  writer_->ExpectWriteDataOk(data1.size());

  net::Error error = cache_writer_->StartCopy(CreateWriteCallback());
  EXPECT_EQ(error, net::ERR_IO_PENDING);
  EXPECT_FALSE(write_complete_);

  SimulateDisconnection();

  EXPECT_EQ(last_error_, net::ERR_FAILED);
  EXPECT_TRUE(write_complete_);
}

TEST_F(ServiceWorkerCacheWriterDisconnectionTest, CopyBeforeDataRead) {
  const std::string data1 = "abcd";
  const size_t response_size = data1.size();

  InitializeForCopy();
  copy_reader_->ExpectReadResponseHeadOk(response_size);
  copy_reader_->ExpectReadDataOk(data1);
  writer_->ExpectWriteResponseHeadOk(response_size);
  writer_->ExpectWriteDataOk(data1.size());

  net::Error error = cache_writer_->StartCopy(CreateWriteCallback());
  EXPECT_EQ(error, net::ERR_IO_PENDING);
  EXPECT_FALSE(write_complete_);

  // Completes the header read.
  copy_reader_->CompletePendingRead();
  EXPECT_EQ(last_error_, net::OK);
  EXPECT_FALSE(write_complete_);

  // Completes the header write.
  writer_->CompletePendingWrite();
  EXPECT_EQ(last_error_, net::OK);
  EXPECT_FALSE(write_complete_);

  SimulateDisconnection();

  EXPECT_EQ(last_error_, net::ERR_FAILED);
  EXPECT_TRUE(write_complete_);
}

TEST_F(ServiceWorkerCacheWriterDisconnectionTest, CopyDuringDataRead) {
  const std::string data1 = "abcd";
  const std::string data2 = "efgh";
  const size_t response_size = data1.size() + data2.size();

  InitializeForCopy();
  copy_reader_->ExpectReadResponseHeadOk(response_size);
  copy_reader_->ExpectReadDataOk(data1);
  copy_reader_->ExpectReadDataOk(data2);
  writer_->ExpectWriteResponseHeadOk(response_size);
  writer_->ExpectWriteDataOk(data1.size());
  writer_->ExpectWriteDataOk(data2.size());

  net::Error error = cache_writer_->StartCopy(CreateWriteCallback());
  EXPECT_EQ(error, net::ERR_IO_PENDING);
  EXPECT_FALSE(write_complete_);

  // Completes the header read.
  copy_reader_->CompletePendingRead();
  EXPECT_EQ(last_error_, net::OK);
  EXPECT_FALSE(write_complete_);

  // Completes the header write.
  writer_->CompletePendingWrite();
  EXPECT_EQ(last_error_, net::OK);
  EXPECT_FALSE(write_complete_);

  // Completes the read of the first data chunk.
  copy_reader_->CompletePendingRead();
  EXPECT_EQ(last_error_, net::OK);
  EXPECT_FALSE(write_complete_);

  // Completes the write of the first data chunk.
  writer_->CompletePendingWrite();
  EXPECT_EQ(last_error_, net::OK);
  EXPECT_FALSE(write_complete_);

  SimulateDisconnection();

  EXPECT_EQ(last_error_, net::ERR_FAILED);
  EXPECT_TRUE(write_complete_);
}

TEST_F(ServiceWorkerCacheWriterDisconnectionTest, ComparisonBeforeHeaderRead) {
  const std::string data1 = "abcd";
  const size_t response_size = data1.size();

  InitializeForComparison(/*pause_when_not_identical=*/false);
  compare_reader_->ExpectReadResponseHeadOk(response_size);
  compare_reader_->ExpectReadDataOk(data1);

  net::Error error = WriteHeaders(response_size);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_FALSE(write_complete_);

  SimulateDisconnection();

  EXPECT_EQ(last_error_, net::ERR_FAILED);
  EXPECT_TRUE(write_complete_);
}

TEST_F(ServiceWorkerCacheWriterDisconnectionTest, ComparisonBeforeDataWrite) {
  const std::string data1 = "abcd";
  const size_t response_size = data1.size();

  InitializeForComparison(/*pause_when_not_identical=*/false);
  compare_reader_->ExpectReadResponseHeadOk(response_size);
  compare_reader_->ExpectReadDataOk(data1);

  // Completes the header read.
  net::Error error = WriteHeaders(response_size);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_FALSE(write_complete_);
  compare_reader_->CompletePendingRead();
  EXPECT_TRUE(write_complete_);
  write_complete_ = false;

  SimulateDisconnection();

  error = WriteData(data1);
  EXPECT_EQ(error, net::ERR_FAILED);
}

// Tests that a comparison fails gracefully when remotes are disconnected during
// copy. See also the comment of ServiceWorkerCacheWriterTest.CompareFailedCopy.
TEST_F(ServiceWorkerCacheWriterDisconnectionTest, ComparisonDuringCopy) {
  const std::string data1 = "abcdef";
  const std::string cache_data2 = "ghijkl";
  const std::string net_data2 = "mnopqr";
  const std::string data3 = "stuvwxyz";
  const size_t cache_response_size =
      data1.size() + cache_data2.size() + data3.size();
  const size_t net_response_size =
      data1.size() + net_data2.size() + data3.size();

  InitializeForComparison(/*pause_when_not_identical=*/false);
  compare_reader_->ExpectReadResponseHeadOk(cache_response_size);
  compare_reader_->ExpectReadDataOk(data1);
  compare_reader_->ExpectReadDataOk(cache_data2);

  copy_reader_->ExpectReadResponseHeadOk(cache_response_size);
  copy_reader_->ExpectReadDataOk(data1);

  writer_->ExpectWriteResponseHeadOk(net_response_size);
  writer_->ExpectWriteDataOk(data1.size());
  writer_->ExpectWriteDataOk(net_data2.size());
  writer_->ExpectWriteDataOk(data3.size());

  // Complete the header comparison.
  net::Error error = WriteHeaders(net_response_size);
  EXPECT_EQ(error, net::ERR_IO_PENDING);
  EXPECT_FALSE(write_complete_);
  compare_reader_->CompletePendingRead();
  EXPECT_TRUE(write_complete_);
  write_complete_ = false;

  // Complete the `data1` comparison.
  error = WriteData(data1);
  EXPECT_EQ(error, net::ERR_IO_PENDING);
  EXPECT_FALSE(write_complete_);
  compare_reader_->CompletePendingRead();
  EXPECT_TRUE(write_complete_);
  write_complete_ = false;

  // Finish the comparison of `cache_data2` and `net_data2`.
  error = WriteData(net_data2);
  EXPECT_EQ(error, net::ERR_IO_PENDING);
  EXPECT_FALSE(write_complete_);
  compare_reader_->CompletePendingRead();
  // `pause_when_not_identical` isn't enabled so the write callback should not
  // be called yet.
  EXPECT_FALSE(write_complete_);

  // At this point, `copy_reader_` is asked to read the header and `data1`.
  // Complete the header and `data1` copy.
  copy_reader_->CompletePendingRead();
  writer_->CompletePendingWrite();
  copy_reader_->CompletePendingRead();
  writer_->CompletePendingWrite();
  // Complete the `net_data2` write.
  writer_->CompletePendingWrite();
  EXPECT_TRUE(write_complete_);
  write_complete_ = false;

  error = WriteData(data3);
  EXPECT_EQ(error, net::ERR_IO_PENDING);

  SimulateDisconnection();

  EXPECT_EQ(last_error_, net::ERR_FAILED);
  EXPECT_TRUE(write_complete_);
}

// Tests that a comparison fails gracefully when remotes are disconnected before
// resuming.
TEST_F(ServiceWorkerCacheWriterDisconnectionTest, ComparisonBeforeResume) {
  const std::string data1 = "abcdef";
  const std::string cache_data2 = "ghijkl";
  const std::string net_data2 = "mnopqr";
  const std::string data3 = "stuvwxyz";
  const size_t cache_response_size =
      data1.size() + cache_data2.size() + data3.size();
  const size_t net_response_size =
      data1.size() + net_data2.size() + data3.size();

  InitializeForComparison(/*pause_when_not_identical=*/true);
  compare_reader_->ExpectReadResponseHeadOk(cache_response_size);
  compare_reader_->ExpectReadDataOk(data1);
  compare_reader_->ExpectReadDataOk(cache_data2);

  copy_reader_->ExpectReadResponseHeadOk(cache_response_size);
  copy_reader_->ExpectReadDataOk(data1);

  writer_->ExpectWriteResponseHeadOk(net_response_size);
  writer_->ExpectWriteDataOk(data1.size());
  writer_->ExpectWriteDataOk(net_data2.size());
  writer_->ExpectWriteDataOk(data3.size());

  // Complete the header comparison.
  net::Error error = WriteHeaders(net_response_size);
  EXPECT_EQ(error, net::ERR_IO_PENDING);
  EXPECT_FALSE(write_complete_);
  compare_reader_->CompletePendingRead();
  EXPECT_TRUE(write_complete_);
  write_complete_ = false;

  // Complete the `data1` comparison.
  error = WriteData(data1);
  EXPECT_EQ(error, net::ERR_IO_PENDING);
  EXPECT_FALSE(write_complete_);
  compare_reader_->CompletePendingRead();
  EXPECT_TRUE(write_complete_);
  write_complete_ = false;

  // Finish the comparison of `cache_data2` and `net_data2`.
  error = WriteData(net_data2);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_FALSE(write_complete_);
  compare_reader_->CompletePendingRead();
  // `pause_when_not_identical` is enabled so the write callback should be
  // called.
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(last_error_, net::ERR_IO_PENDING);

  SimulateDisconnection();

  error = WriteData(data3);
  EXPECT_EQ(error, net::ERR_FAILED);
}

}  // namespace
}  // namespace content
