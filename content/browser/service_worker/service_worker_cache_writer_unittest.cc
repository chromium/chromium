// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_writer.h"

#include <stddef.h>

#include <list>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

// A test implementation of ServiceWorkerCacheWriter::WriteObserver.
// This observer captures the response info or data sent to the observer
// for further checking.
class MockServiceWorkerCacheWriterObserver
    : public ServiceWorkerCacheWriter::WriteObserver {
 public:
  MockServiceWorkerCacheWriterObserver() : data_length_(0), result_(net::OK) {}
  ~MockServiceWorkerCacheWriterObserver() {}

  int WillWriteInfo(
      scoped_refptr<HttpResponseInfoIOBuffer> response_info) override {
    response_info_ = std::move(response_info);
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

  scoped_refptr<HttpResponseInfoIOBuffer> response_info_;
  scoped_refptr<net::IOBuffer> data_;
  size_t data_length_;
  base::OnceCallback<void(net::Error)> callback_;
  net::Error result_;

  DISALLOW_COPY_AND_ASSIGN(MockServiceWorkerCacheWriterObserver);
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
  ~ServiceWorkerCacheWriterTest() override {}

  MockServiceWorkerResponseReader* ExpectReader() {
    auto reader = std::make_unique<MockServiceWorkerResponseReader>();
    MockServiceWorkerResponseReader* borrowed_reader = reader.get();
    readers_.push_back(std::move(reader));
    return borrowed_reader;
  }

  MockServiceWorkerResponseWriter* ExpectWriter() {
    auto writer = std::make_unique<MockServiceWorkerResponseWriter>();
    MockServiceWorkerResponseWriter* borrowed_writer = writer.get();
    writers_.push_back(std::move(writer));
    return borrowed_writer;
  }

  // This should be called after ExpectReader() and ExpectWriter().
  void Initialize(CacheWriterUsage type, bool pause_when_not_identical) {
    switch (type) {
      case CacheWriterUsage::kForCopy:
        cache_writer_ = ServiceWorkerCacheWriter::CreateForCopy(CreateReader(),
                                                                CreateWriter());
        break;
      case CacheWriterUsage::kForWriteBack:
        cache_writer_ =
            ServiceWorkerCacheWriter::CreateForWriteBack(CreateWriter());
        break;
      case CacheWriterUsage::kForComparison:
        auto compare_reader = CreateReader();
        auto copy_reader = CreateReader();
        cache_writer_ = ServiceWorkerCacheWriter::CreateForComparison(
            std::move(compare_reader), std::move(copy_reader), CreateWriter(),
            pause_when_not_identical);
        break;
    };
  }

 protected:
  std::list<std::unique_ptr<MockServiceWorkerResponseReader>> readers_;
  std::list<std::unique_ptr<MockServiceWorkerResponseWriter>> writers_;
  std::unique_ptr<ServiceWorkerCacheWriter> cache_writer_;
  bool write_complete_ = false;
  net::Error last_error_;

  std::unique_ptr<ServiceWorkerResponseReader> CreateReader() {
    if (readers_.empty())
      return base::WrapUnique<ServiceWorkerResponseReader>(nullptr);
    std::unique_ptr<ServiceWorkerResponseReader> reader(
        std::move(readers_.front()));
    readers_.pop_front();
    return reader;
  }

  std::unique_ptr<ServiceWorkerResponseWriter> CreateWriter() {
    if (writers_.empty())
      return base::WrapUnique<ServiceWorkerResponseWriter>(nullptr);
    std::unique_ptr<ServiceWorkerResponseWriter> writer(
        std::move(writers_.front()));
    writers_.pop_front();
    return writer;
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
    scoped_refptr<HttpResponseInfoIOBuffer> buf(new HttpResponseInfoIOBuffer);
    buf->response_data_size = len;
    return cache_writer_->MaybeWriteHeaders(buf.get(), CreateWriteCallback());
  }

  net::Error WriteData(const std::string& data) {
    scoped_refptr<net::IOBuffer> buf =
        base::MakeRefCounted<net::StringIOBuffer>(data);
    return cache_writer_->MaybeWriteData(buf.get(), data.size(),
                                         CreateWriteCallback());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerCacheWriterTest);
};

// Passthrough tests:
// In these tests, the ServiceWorkerCacheWriter under test has no existing
// reader, since no calls to ExpectReader() have been made; this means that
// there is no existing cached response and the incoming data is written back to
// the cache directly.

TEST_F(ServiceWorkerCacheWriterTest, PassthroughHeadersSync) {
  const size_t kHeaderSize = 16;
  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  writer->ExpectWriteInfoOk(kHeaderSize, false);
  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_EQ(net::OK, error);
  EXPECT_FALSE(write_complete_);
  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_EQ(0U, cache_writer_->bytes_written());
}

TEST_F(ServiceWorkerCacheWriterTest, PassthroughHeadersAsync) {
  size_t kHeaderSize = 16;
  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  writer->ExpectWriteInfoOk(kHeaderSize, true);
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

TEST_F(ServiceWorkerCacheWriterTest, PassthroughDataSync) {
  const std::string data1 = "abcdef";
  const std::string data2 = "ghijklmno";
  size_t response_size = data1.size() + data2.size();

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  writer->ExpectWriteInfoOk(response_size, false);
  writer->ExpectWriteDataOk(data1.size(), false);
  writer->ExpectWriteDataOk(data2.size(), false);
  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(response_size);
  EXPECT_EQ(net::OK, error);

  error = WriteData(data1);
  EXPECT_EQ(net::OK, error);

  error = WriteData(data2);
  EXPECT_EQ(net::OK, error);
  EXPECT_TRUE(writer->AllExpectedWritesDone());
}

TEST_F(ServiceWorkerCacheWriterTest, PassthroughDataAsync) {
  const std::string data1 = "abcdef";
  const std::string data2 = "ghijklmno";
  size_t response_size = data1.size() + data2.size();

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  writer->ExpectWriteInfoOk(response_size, false);
  writer->ExpectWriteDataOk(data1.size(), true);
  writer->ExpectWriteDataOk(data2.size(), true);
  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(response_size);
  EXPECT_EQ(net::OK, error);

  error = WriteData(data1);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  writer->CompletePendingWrite();
  EXPECT_TRUE(write_complete_);

  write_complete_ = false;
  error = WriteData(data2);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  writer->CompletePendingWrite();
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(net::OK, last_error_);
  EXPECT_TRUE(writer->AllExpectedWritesDone());
}

TEST_F(ServiceWorkerCacheWriterTest, PassthroughHeadersFailSync) {
  const size_t kHeaderSize = 16;
  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  writer->ExpectWriteInfo(kHeaderSize, false, net::ERR_FAILED);
  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_EQ(net::ERR_FAILED, error);
  EXPECT_FALSE(write_complete_);
  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_EQ(0U, cache_writer_->bytes_written());
}

TEST_F(ServiceWorkerCacheWriterTest, PassthroughHeadersFailAsync) {
  size_t kHeaderSize = 16;
  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  writer->ExpectWriteInfo(kHeaderSize, true, net::ERR_FAILED);
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

TEST_F(ServiceWorkerCacheWriterTest, PassthroughDataFailSync) {
  const std::string data = "abcdef";

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  writer->ExpectWriteInfoOk(data.size(), false);
  writer->ExpectWriteData(data.size(), false, net::ERR_FAILED);
  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);

  EXPECT_EQ(net::OK, WriteHeaders(data.size()));
  EXPECT_EQ(net::ERR_FAILED, WriteData(data));
  EXPECT_TRUE(writer->AllExpectedWritesDone());
}

TEST_F(ServiceWorkerCacheWriterTest, PassthroughDataFailAsync) {
  const std::string data = "abcdef";

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  writer->ExpectWriteInfoOk(data.size(), false);
  writer->ExpectWriteData(data.size(), true, net::ERR_FAILED);
  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);

  EXPECT_EQ(net::OK, WriteHeaders(data.size()));

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

TEST_F(ServiceWorkerCacheWriterTest, CompareHeadersSync) {
  size_t response_size = 3;
  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  MockServiceWorkerResponseReader* reader = ExpectReader();

  // Create a copy reader as it's needed to create cache writer for comparison
  // though not used in this test.
  ExpectReader();

  reader->ExpectReadInfoOk(response_size, false);
  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(response_size);
  EXPECT_EQ(net::OK, error);
  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_TRUE(reader->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerCacheWriterTest, CompareDataOkSync) {
  const std::string data1 = "abcdef";
  size_t response_size = data1.size();

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  MockServiceWorkerResponseReader* reader = ExpectReader();

  // Create a copy reader as it's needed to create cache writer for comparison
  // though not used in this test.
  ExpectReader();

  reader->ExpectReadInfoOk(response_size, false);
  reader->ExpectReadDataOk(data1, false);
  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(response_size);
  EXPECT_EQ(net::OK, error);

  error = WriteData(data1);
  EXPECT_EQ(net::OK, error);

  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_TRUE(reader->AllExpectedReadsDone());
  EXPECT_EQ(0U, cache_writer_->bytes_written());
}

TEST_F(ServiceWorkerCacheWriterTest, CompareHeadersFailSync) {
  size_t response_size = 3;
  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  MockServiceWorkerResponseReader* reader = ExpectReader();

  // Create a copy reader as it's needed to create cache writer for comparison
  // though not used in this test.
  ExpectReader();

  reader->ExpectReadInfo(response_size, false, net::ERR_FAILED);
  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  EXPECT_EQ(net::ERR_FAILED, WriteHeaders(response_size));
  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_TRUE(reader->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerCacheWriterTest, CompareDataFailSync) {
  const std::string data1 = "abcdef";
  size_t response_size = data1.size();

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  MockServiceWorkerResponseReader* reader = ExpectReader();

  // Create a copy reader as it's needed to create cache writer for comparison
  // though not used in this test.
  ExpectReader();

  reader->ExpectReadInfoOk(response_size, false);
  reader->ExpectReadData(data1.c_str(), data1.length(), false, net::ERR_FAILED);
  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(response_size);
  EXPECT_EQ(net::OK, error);

  EXPECT_EQ(net::ERR_FAILED, WriteData(data1));

  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_TRUE(reader->AllExpectedReadsDone());
  EXPECT_EQ(0U, cache_writer_->bytes_written());
}

TEST_F(ServiceWorkerCacheWriterTest, CompareShortCacheReads) {
  const size_t kHeaderSize = 16;
  const std::string& data1 = "abcdef";
  const std::string& cache_data2 = "ghi";
  const std::string& cache_data3 = "j";
  const std::string& cache_data4 = "kl";
  const std::string& net_data2 = "ghijkl";
  const std::string& data5 = "mnopqrst";

  MockServiceWorkerResponseReader* reader = ExpectReader();
  reader->ExpectReadInfo(kHeaderSize, false, kHeaderSize);
  reader->ExpectReadDataOk(data1, false);
  reader->ExpectReadDataOk(cache_data2, false);
  reader->ExpectReadDataOk(cache_data3, false);
  reader->ExpectReadDataOk(cache_data4, false);
  reader->ExpectReadDataOk(data5, false);

  // Create a copy reader and writer as they're needed to create cache writer
  // for comparison though not used in this test.
  ExpectReader();
  ExpectWriter();

  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_EQ(net::OK, error);
  error = WriteData(data1);
  EXPECT_EQ(net::OK, error);
  error = WriteData(net_data2);
  EXPECT_EQ(net::OK, error);
  error = WriteData(data5);
  EXPECT_EQ(net::OK, error);
  EXPECT_TRUE(reader->AllExpectedReadsDone());
  EXPECT_EQ(0U, cache_writer_->bytes_written());
}

TEST_F(ServiceWorkerCacheWriterTest, CompareDataOkAsync) {
  const std::string data1 = "abcdef";
  size_t response_size = data1.size();

  MockServiceWorkerResponseReader* reader = ExpectReader();

  // Create a copy reader and writer as they're needed to create cache writer
  // for comparison though not used in this test.
  ExpectReader();
  ExpectWriter();

  reader->ExpectReadInfoOk(response_size, true);
  reader->ExpectReadDataOk(data1, true);
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
      "abcdef", "ghijkl", "mnopqr", "stuvwxyz",
  };
  size_t response_size = 0;
  for (size_t i = 0; i < base::size(expected_data); ++i)
    response_size += expected_data[i].size();

  MockServiceWorkerResponseReader* reader = ExpectReader();

  // Create a copy reader and writer as they're needed to create cache writer
  // for comparison though not used in this test.
  ExpectReader();
  ExpectWriter();

  reader->ExpectReadInfoOk(response_size, true);
  for (size_t i = 0; i < base::size(expected_data); ++i) {
    reader->ExpectReadDataOk(expected_data[i], true);
  }
  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(response_size);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  reader->CompletePendingRead();

  for (size_t i = 0; i < base::size(expected_data); ++i) {
    error = WriteData(expected_data[i]);
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
TEST_F(ServiceWorkerCacheWriterTest, CompareFailedCopySync) {
  std::string data1 = "abcdef";
  std::string cache_data2 = "ghijkl";
  std::string net_data2 = "mnopqr";
  std::string data3 = "stuvwxyz";
  size_t cache_response_size = data1.size() + cache_data2.size() + data3.size();
  size_t net_response_size = data1.size() + net_data2.size() + data3.size();

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  MockServiceWorkerResponseReader* compare_reader = ExpectReader();
  MockServiceWorkerResponseReader* copy_reader = ExpectReader();

  compare_reader->ExpectReadInfoOk(cache_response_size, false);
  compare_reader->ExpectReadDataOk(data1, false);
  compare_reader->ExpectReadDataOk(cache_data2, false);

  copy_reader->ExpectReadInfoOk(cache_response_size, false);
  copy_reader->ExpectReadDataOk(data1, false);

  writer->ExpectWriteInfoOk(net_response_size, false);
  writer->ExpectWriteDataOk(data1.size(), false);
  writer->ExpectWriteDataOk(net_data2.size(), false);
  writer->ExpectWriteDataOk(data3.size(), false);

  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(net_response_size);
  EXPECT_EQ(net::OK, error);
  error = WriteData(data1);
  EXPECT_EQ(net::OK, error);
  error = WriteData(net_data2);
  EXPECT_EQ(net::OK, error);
  error = WriteData(data3);
  EXPECT_EQ(net::OK, error);

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

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  MockServiceWorkerResponseReader* compare_reader = ExpectReader();
  MockServiceWorkerResponseReader* copy_reader = ExpectReader();

  compare_reader->ExpectReadInfoOk(cache_response_size, false);
  compare_reader->ExpectReadDataOk(data1, false);
  compare_reader->ExpectReadDataOk(cache_data2, false);
  compare_reader->ExpectReadDataOk("", false);  // EOF read

  copy_reader->ExpectReadInfoOk(cache_response_size, false);
  copy_reader->ExpectReadDataOk(data1, false);

  writer->ExpectWriteInfoOk(net_response_size, false);
  writer->ExpectWriteDataOk(data1.size(), false);
  writer->ExpectWriteDataOk(net_data2.size(), false);
  writer->ExpectWriteDataOk(data3.size(), false);

  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(net_response_size);
  EXPECT_EQ(net::OK, error);
  error = WriteData(data1);
  EXPECT_EQ(net::OK, error);
  error = WriteData(net_data2);
  EXPECT_EQ(net::OK, error);
  error = WriteData(data3);
  EXPECT_EQ(net::OK, error);

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

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  MockServiceWorkerResponseReader* compare_reader = ExpectReader();
  MockServiceWorkerResponseReader* copy_reader = ExpectReader();

  compare_reader->ExpectReadInfoOk(cached_size, false);
  compare_reader->ExpectReadDataOk(data1, false);
  compare_reader->ExpectReadDataOk(cache_data2, false);

  // The comparison should fail at the end of |cache_data2|, when the cache
  // writer realizes the two responses are different sizes, and then the network
  // data should be written back starting with |net_data2|.
  copy_reader->ExpectReadInfoOk(cached_size, false);
  copy_reader->ExpectReadDataOk(data1, false);
  copy_reader->ExpectReadDataOk(net_data2, false);

  writer->ExpectWriteInfoOk(net_size, false);
  writer->ExpectWriteDataOk(data1.size(), false);
  writer->ExpectWriteDataOk(net_data2.size(), false);

  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(net_size);
  EXPECT_EQ(net::OK, error);
  error = WriteData(data1);
  EXPECT_EQ(net::OK, error);
  error = WriteData(net_data2);
  EXPECT_EQ(net::OK, error);
  error = WriteData("");
  EXPECT_EQ(net::OK, error);

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

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  MockServiceWorkerResponseReader* compare_reader = ExpectReader();
  MockServiceWorkerResponseReader* copy_reader = ExpectReader();

  compare_reader->ExpectReadInfoOk(bytes_cached, false);
  for (const auto& data : data_from_cache)
    compare_reader->ExpectReadDataOk(data, false);

  copy_reader->ExpectReadInfoOk(bytes_common, false);
  for (const auto& data : data_to_copy)
    copy_reader->ExpectReadDataOk(data, false);

  writer->ExpectWriteInfoOk(bytes_from_net, false);
  for (const auto& data : data_expected)
    writer->ExpectWriteDataOk(data.size(), false);

  Initialize(CacheWriterUsage::kForComparison,
             false /* pause_when_not_identical */);

  net::Error error = WriteHeaders(bytes_from_net);
  EXPECT_EQ(net::OK, error);
  for (const auto& data : data_from_net) {
    error = WriteData(data);
    EXPECT_EQ(net::OK, error);
  }

  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_TRUE(compare_reader->AllExpectedReadsDone());
  EXPECT_TRUE(copy_reader->AllExpectedReadsDone());
}

// Tests behavior when |pause_when_not_identical| is enabled and cache writer
// finishes synchronously.
TEST_F(ServiceWorkerCacheWriterTest, PauseWhenNotIdentical_SyncWriteData) {
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

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  MockServiceWorkerResponseReader* compare_reader = ExpectReader();
  MockServiceWorkerResponseReader* copy_reader = ExpectReader();

  compare_reader->ExpectReadInfoOk(bytes_cached, false);
  for (const auto& data : data_from_cache)
    compare_reader->ExpectReadDataOk(data, false);

  copy_reader->ExpectReadInfoOk(bytes_cached, false);

  writer->ExpectWriteInfoOk(bytes_expected, false);
  for (const auto& data : data_expected)
    writer->ExpectWriteDataOk(data.size(), false);

  Initialize(CacheWriterUsage::kForComparison,
             true /* pause_when_not_identical */);

  net::Error error = WriteHeaders(bytes_from_net);
  EXPECT_EQ(net::OK, error);

  // |cache_writer_| stops the comparison at the first block of the data.
  // It should return net::ERR_IO_PENDING and |write_complete_| should remain
  // false since |pause_when_not_identical| forbids proceeding to the next step.
  write_complete_ = false;
  error = WriteData(data_from_net[0]);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_FALSE(write_complete_);
  EXPECT_EQ(0U, cache_writer_->bytes_written());

  // Resume |cache_writer_| with a callback. The passed callback shouldn't be
  // called because this is a synchronous write. This time, the result should be
  // net::OK and the expected data should be written to the storage.
  error =
      cache_writer_->Resume(base::BindOnce([](net::Error) { NOTREACHED(); }));
  EXPECT_EQ(net::OK, error);
  EXPECT_EQ(bytes_expected, cache_writer_->bytes_written());

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

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  MockServiceWorkerResponseReader* compare_reader = ExpectReader();
  MockServiceWorkerResponseReader* copy_reader = ExpectReader();

  compare_reader->ExpectReadInfoOk(bytes_cached, true);
  for (const auto& data : data_from_cache)
    compare_reader->ExpectReadDataOk(data, true);

  copy_reader->ExpectReadInfoOk(bytes_cached, true);

  writer->ExpectWriteInfoOk(bytes_expected, true);
  for (const auto& data : data_expected)
    writer->ExpectWriteDataOk(data.size(), true);

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

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  MockServiceWorkerResponseReader* copy_reader = ExpectReader();

  copy_reader->ExpectReadInfoOk(bytes_cached, true);
  for (const auto& data : data_from_cache)
    copy_reader->ExpectReadDataOk(data, true);

  writer->ExpectWriteInfoOk(bytes_expected, true);
  for (const auto& data : data_expected)
    writer->ExpectWriteDataOk(data.size(), true);

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

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  MockServiceWorkerResponseReader* copy_reader = ExpectReader();

  copy_reader->ExpectReadInfoOk(bytes_cached, true);
  for (const auto& data : data_from_cache)
    copy_reader->ExpectReadDataOk(data, true);

  writer->ExpectWriteInfoOk(bytes_expected, true);
  for (const auto& data : data_expected)
    writer->ExpectWriteDataOk(data.size(), true);

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

// Tests behavior of a cache writer used to copy script which finishes
// synchronously.
TEST_F(ServiceWorkerCacheWriterTest, CopyScript_Sync) {
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

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  MockServiceWorkerResponseReader* copy_reader = ExpectReader();

  copy_reader->ExpectReadInfoOk(bytes_cached, false);
  for (const auto& data : data_from_cache)
    copy_reader->ExpectReadDataOk(data, false);

  writer->ExpectWriteInfoOk(bytes_expected, false);
  for (const auto& data : data_expected)
    writer->ExpectWriteDataOk(data.size(), false);

  Initialize(CacheWriterUsage::kForCopy, false /* pause_when_not_identical */);

  net::Error error = cache_writer_->StartCopy(CreateWriteCallback());

  // In synchronous read and write, everything finishes synchronously.
  EXPECT_EQ(net::OK, error);
  EXPECT_EQ(bytes_expected, cache_writer_->bytes_written());
  EXPECT_TRUE(copy_reader->AllExpectedReadsDone());
  EXPECT_TRUE(writer->AllExpectedWritesDone());
}

// Tests behavior of a cache writer used to copy script that read multiple
// times and finishes synchronously.
TEST_F(ServiceWorkerCacheWriterTest, CopyScript_SyncMultiRead) {
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

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  MockServiceWorkerResponseReader* copy_reader = ExpectReader();

  copy_reader->ExpectReadInfoOk(bytes_cached, false);
  for (const auto& data : data_from_cache)
    copy_reader->ExpectReadDataOk(data, false);

  writer->ExpectWriteInfoOk(bytes_expected, false);
  for (const auto& data : data_expected)
    writer->ExpectWriteDataOk(data.size(), false);

  Initialize(CacheWriterUsage::kForCopy, false /* pause_when_not_identical */);

  net::Error error = cache_writer_->StartCopy(CreateWriteCallback());

  // In synchronous read and write, everything finishes synchronously.
  EXPECT_EQ(net::OK, error);
  EXPECT_EQ(bytes_expected, cache_writer_->bytes_written());
  EXPECT_TRUE(copy_reader->AllExpectedReadsDone());
  EXPECT_TRUE(writer->AllExpectedWritesDone());
}

// The observer and the response writer all run synchronously.
TEST_F(ServiceWorkerCacheWriterTest, ObserverSyncResponseWriterSync) {
  const size_t kHeaderSize = 16;
  const std::string data = "abcdef";
  size_t response_size = data.size();

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  writer->ExpectWriteInfoOk(kHeaderSize, false);
  writer->ExpectWriteDataOk(data.size(), false);

  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);
  MockServiceWorkerCacheWriterObserver observer;
  cache_writer_->set_write_observer(&observer);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_TRUE(observer.response_info_);
  EXPECT_EQ(net::OK, error);

  error = WriteData(data);
  EXPECT_EQ(net::OK, error);
  EXPECT_EQ(observer.data_length_, response_size);
  EXPECT_TRUE(observer.data_);

  cache_writer_->set_write_observer(nullptr);

  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_EQ(response_size, cache_writer_->bytes_written());
}

// The observer runs asynchronously and the response writer runs synchronously.
TEST_F(ServiceWorkerCacheWriterTest, ObserverAsyncResponseWriterSync) {
  const size_t kHeaderSize = 16;
  const std::string data = "abcdef";
  size_t response_size = data.size();

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  writer->ExpectWriteInfoOk(kHeaderSize, false);
  writer->ExpectWriteDataOk(data.size(), false);

  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);
  MockServiceWorkerCacheWriterObserver observer;
  cache_writer_->set_write_observer(&observer);
  observer.set_result(net::ERR_IO_PENDING);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_EQ(net::OK, error);
  EXPECT_TRUE(observer.response_info_);

  error = WriteData(data);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  observer.Complete(net::OK);
  EXPECT_EQ(observer.data_length_, response_size);
  EXPECT_TRUE(observer.data_);
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(last_error_, net::OK);

  cache_writer_->set_write_observer(nullptr);

  EXPECT_TRUE(writer->AllExpectedWritesDone());
  EXPECT_EQ(response_size, cache_writer_->bytes_written());
}

// The observer runs synchronously and the response writer runs asynchronously.
TEST_F(ServiceWorkerCacheWriterTest, ObserverSyncResponseWriterAsync) {
  const size_t kHeaderSize = 16;
  const std::string data = "abcdef";
  size_t response_size = data.size();

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  writer->ExpectWriteInfoOk(kHeaderSize, true);
  writer->ExpectWriteDataOk(data.size(), true);

  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);
  MockServiceWorkerCacheWriterObserver observer;
  cache_writer_->set_write_observer(&observer);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_TRUE(observer.response_info_);
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

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  writer->ExpectWriteInfoOk(kHeaderSize, true);
  writer->ExpectWriteDataOk(data.size(), true);

  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);
  MockServiceWorkerCacheWriterObserver observer;
  cache_writer_->set_write_observer(&observer);
  observer.set_result(net::ERR_IO_PENDING);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_TRUE(observer.response_info_);
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

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  writer->ExpectWriteInfoOk(kHeaderSize, false);
  writer->ExpectWriteDataOk(data.size(), false);

  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);
  MockServiceWorkerCacheWriterObserver observer;
  cache_writer_->set_write_observer(&observer);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_TRUE(observer.response_info_);
  EXPECT_EQ(net::OK, error);

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

  MockServiceWorkerResponseWriter* writer = ExpectWriter();
  writer->ExpectWriteInfoOk(kHeaderSize, false);
  writer->ExpectWriteDataOk(data.size(), false);

  Initialize(CacheWriterUsage::kForWriteBack,
             false /* pause_when_not_identical */);
  MockServiceWorkerCacheWriterObserver observer;
  cache_writer_->set_write_observer(&observer);
  observer.set_result(net::ERR_IO_PENDING);

  net::Error error = WriteHeaders(kHeaderSize);
  EXPECT_EQ(net::OK, error);
  EXPECT_TRUE(observer.response_info_);

  error = WriteData(data);
  EXPECT_EQ(net::ERR_IO_PENDING, error);
  observer.Complete(net::ERR_FAILED);
  EXPECT_TRUE(write_complete_);
  EXPECT_EQ(last_error_, net::ERR_FAILED);
  EXPECT_EQ(0U, cache_writer_->bytes_written());

  cache_writer_->set_write_observer(nullptr);
}

}  // namespace
}  // namespace content
