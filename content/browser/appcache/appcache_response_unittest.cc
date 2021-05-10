// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/containers/stack.h"
#include "base/location.h"
#include "base/message_loop/message_pump_type.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/escape.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "content/browser/appcache/appcache_response_info.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::IOBuffer;
using net::WrappedIOBuffer;

namespace content {

namespace {

constexpr int kNumBlocks = 4;
constexpr int kBlockSize = 1024;
constexpr int kNoSuchResponseId = 123;

}  // namespace

class AppCacheResponseTest : public testing::Test {
 public:
  // Test Harness -------------------------------------------------------------

  // Helper class used to verify test results
  class MockStorageDelegate : public AppCacheStorage::Delegate {
   public:
    explicit MockStorageDelegate(
        AppCacheResponseTest* test,
        base::OnceClosure response_info_loaded_callback)
        : loaded_info_id_(0),
          test_(test),
          response_info_loaded_callback_(
              std::move(response_info_loaded_callback)) {}

    void OnResponseInfoLoaded(AppCacheResponseInfo* info,
                              int64_t response_id) override {
      loaded_info_ = info;
      loaded_info_id_ = response_id;
      std::move(response_info_loaded_callback_).Run();
    }

    scoped_refptr<AppCacheResponseInfo> loaded_info_;
    int64_t loaded_info_id_;
    AppCacheResponseTest* test_;
    base::OnceClosure response_info_loaded_callback_;
  };

  enum class CallbackMode {
    kPostTask,
    kImmediate,
  };

  AppCacheResponseTest() = default;
  ~AppCacheResponseTest() override = default;

  template <class Method>
  void RunTestOnIOThread(Method method) {
    (this->*method)();
  }

  void SetUp() override {
    task_environment_ = std::make_unique<content::BrowserTaskEnvironment>();
    service_ = std::make_unique<MockAppCacheService>();
  }

  // Wrappers to call AppCacheResponseReader/Writer Read and Write methods

  void WriteBasicResponse(base::OnceClosure callback) {
    static const char kHttpHeaders[] =
        "HTTP/1.0 200 OK\0Content-Length: 5\0\0";
    static const char kHttpBody[] = "Hello";
    scoped_refptr<IOBuffer> body =
        base::MakeRefCounted<WrappedIOBuffer>(kHttpBody);
    std::string raw_headers(kHttpHeaders, base::size(kHttpHeaders));
    WriteResponse(MakeHttpResponseInfo(raw_headers), body.get(),
                  strlen(kHttpBody), std::move(callback));
  }

  int basic_response_size() { return 5; }  // should match kHttpBody above

  void WriteResponse(std::unique_ptr<net::HttpResponseInfo> head,
                     IOBuffer* body,
                     int body_len,
                     base::OnceClosure callback) {
    DCHECK(body);
    scoped_refptr<IOBuffer> body_ref(body);
    WriteResponseHead(
        std::move(head),
        base::BindOnce(&AppCacheResponseTest::WriteResponseBody,
                       base::Unretained(this), body_ref, body_len,
                       CallbackMode::kPostTask, std::move(callback)));
  }

  void WriteResponseHead(std::unique_ptr<net::HttpResponseInfo> head,
                         base::OnceClosure callback) {
    EXPECT_FALSE(writer_->IsWritePending());
    expected_write_result_ = GetHttpResponseInfoSize(*head);
    write_info_buffer_ =
        base::MakeRefCounted<HttpResponseInfoIOBuffer>(std::move(head));
    writer_->WriteInfo(
        write_info_buffer_.get(),
        base::BindOnce(&AppCacheResponseTest::OnWriteInfoComplete,
                       base::Unretained(this), std::move(callback)));
  }

  void WriteResponseBody(scoped_refptr<IOBuffer> io_buffer,
                         int buf_len,
                         CallbackMode callback_mode,
                         base::OnceClosure callback) {
    EXPECT_FALSE(writer_->IsWritePending());
    write_buffer_ = std::move(io_buffer);
    expected_write_result_ = buf_len;
    writer_->WriteData(write_buffer_.get(), buf_len,
                       base::BindOnce(&AppCacheResponseTest::OnWriteComplete,
                                      base::Unretained(this), callback_mode,
                                      std::move(callback)));
  }

  void WriteResponseMetadata(scoped_refptr<IOBuffer> io_buffer,
                             int buf_len,
                             base::OnceClosure callback) {
    EXPECT_FALSE(metadata_writer_->IsWritePending());
    write_buffer_ = io_buffer;
    expected_write_result_ = buf_len;
    metadata_writer_->WriteMetadata(
        write_buffer_.get(), buf_len,
        base::BindOnce(&AppCacheResponseTest::OnMetadataWriteComplete,
                       base::Unretained(this), std::move(callback)));
  }

  void ReadResponseBody(scoped_refptr<IOBuffer> io_buffer,
                        int buf_len,
                        CallbackMode callback_mode,
                        base::OnceClosure callback) {
    EXPECT_FALSE(reader_->IsReadPending());
    read_buffer_ = io_buffer;
    expected_read_result_ = buf_len;
    reader_->ReadData(read_buffer_.get(), buf_len,
                      base::BindOnce(&AppCacheResponseTest::OnReadComplete,
                                     base::Unretained(this), callback_mode,
                                     std::move(callback)));
  }

  void WriteOneBlock(int block_number,
                     CallbackMode callback_mode,
                     base::OnceClosure callback) {
    scoped_refptr<IOBuffer> io_buffer =
        base::MakeRefCounted<IOBuffer>(kBlockSize);
    FillData(block_number, io_buffer->data(), kBlockSize);
    WriteResponseBody(io_buffer, kBlockSize, callback_mode,
                      std::move(callback));
  }

  void WriteOutBlocks(base::OnceClosure callback) {
    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();

    for (int i = 0; i < kNumBlocks; ++i) {
      callback = base::BindOnce(&AppCacheResponseTest::WriteOneBlock,
                                base::Unretained(this), kNumBlocks - i,
                                CallbackMode::kPostTask, std::move(callback));
    }
    std::move(callback).Run();
  }

  void ReadInBlocks(base::OnceClosure callback) {
    writer_.reset();
    reader_ =
        service_->storage()->CreateResponseReader(GURL(), written_response_id_);
    for (int i = 0; i < kNumBlocks; ++i) {
      callback = base::BindOnce(&AppCacheResponseTest::ReadOneBlock,
                                base::Unretained(this), kNumBlocks - i,
                                std::move(callback));
    }
    std::move(callback).Run();
  }

  void ReadOneBlock(int block_number, base::OnceClosure read_verify_callback) {
    ReadResponseBody(base::MakeRefCounted<IOBuffer>(kBlockSize), kBlockSize,
                     CallbackMode::kPostTask,
                     base::BindOnce(&AppCacheResponseTest::VerifyOneBlock,
                                    base::Unretained(this), block_number,
                                    std::move(read_verify_callback)));
  }

  void VerifyOneBlock(int block_number,
                      base::OnceClosure read_verify_callback) {
    EXPECT_TRUE(CheckData(block_number, read_buffer_->data(), kBlockSize));
    std::move(read_verify_callback).Run();
  }

  // AppCacheResponseReader / Writer completion callbacks

  void OnWriteInfoComplete(base::OnceClosure callback, int result) {
    EXPECT_FALSE(writer_->IsWritePending());
    EXPECT_EQ(expected_write_result_, result);
    GetIOThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
  }

  void OnWriteComplete(CallbackMode callback_mode,
                       base::OnceClosure callback,
                       int result) {
    EXPECT_FALSE(writer_->IsWritePending());
    write_callback_was_called_ = true;
    EXPECT_EQ(expected_write_result_, result);
    if (should_delete_writer_in_completion_callback_ &&
        --writer_deletion_count_down_ == 0) {
      writer_.reset();
    }

    switch (callback_mode) {
      case CallbackMode::kImmediate:
        std::move(callback).Run();
        break;
      case CallbackMode::kPostTask:
        GetIOThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
    }
  }

  void OnMetadataWriteComplete(base::OnceClosure callback, int result) {
    EXPECT_FALSE(metadata_writer_->IsWritePending());
    EXPECT_EQ(expected_write_result_, result);
    GetIOThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
  }

  void OnReadComplete(CallbackMode callback_mode,
                      base::OnceClosure callback,
                      int result) {
    EXPECT_FALSE(reader_->IsReadPending());
    read_callback_was_called_ = true;
    EXPECT_EQ(expected_read_result_, result);
    if (should_delete_reader_in_completion_callback_ &&
        --reader_deletion_count_down_ == 0) {
      reader_.reset();
    }

    switch (callback_mode) {
      case CallbackMode::kImmediate:
        std::move(callback).Run();
        break;
      case CallbackMode::kPostTask:
        GetIOThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
    }
  }

  // Helpers to work with HttpResponseInfo objects

  std::unique_ptr<net::HttpResponseInfo> MakeHttpResponseInfo(
      const std::string& raw_headers) {
    std::unique_ptr<net::HttpResponseInfo> info =
        std::make_unique<net::HttpResponseInfo>();
    info->request_time = base::Time::Now();
    info->response_time = base::Time::Now();
    info->was_cached = false;
    info->headers = base::MakeRefCounted<net::HttpResponseHeaders>(raw_headers);
    return info;
  }

  int GetHttpResponseInfoSize(const net::HttpResponseInfo& info) {
    base::Pickle pickle = PickleHttpResonseInfo(info);
    return pickle.size();
  }

  bool CompareHttpResponseInfos(const net::HttpResponseInfo& info1,
                                const net::HttpResponseInfo& info2) {
    base::Pickle pickle1 = PickleHttpResonseInfo(info1);
    base::Pickle pickle2 = PickleHttpResonseInfo(info2);
    return (pickle1.size() == pickle2.size()) &&
           (0 == memcmp(pickle1.data(), pickle2.data(), pickle1.size()));
  }

  base::Pickle PickleHttpResonseInfo(const net::HttpResponseInfo& info) {
    const bool kSkipTransientHeaders = true;
    const bool kTruncated = false;
    base::Pickle pickle;
    info.Persist(&pickle, kSkipTransientHeaders, kTruncated);
    return pickle;
  }

  // Helpers to fill and verify blocks of memory with a value

  void FillData(char value, char* data, int data_len) {
    memset(data, value, data_len);
  }

  bool CheckData(char value, const char* data, int data_len) {
    for (int i = 0; i < data_len; ++i, ++data) {
      if (*data != value)
        return false;
    }
    return true;
  }

  // Individual Tests ---------------------------------------------------------
  // Most of the individual tests involve multiple async steps. Each test
  // is delineated with a section header.


  // ReadNonExistentResponse -------------------------------------------
  void ReadNonExistentResponse() {
    // 1. Attempt to ReadInfo
    // 2. Attempt to ReadData

    reader_ =
        service_->storage()->CreateResponseReader(GURL(), kNoSuchResponseId);

    base::RunLoop run_loop;
    ReadNonExistentInfo(run_loop.QuitClosure());
    run_loop.Run();
  }

  void ReadNonExistentInfo(base::OnceClosure done_callback) {
    EXPECT_FALSE(reader_->IsReadPending());
    read_info_buffer_ = base::MakeRefCounted<HttpResponseInfoIOBuffer>();
    reader_->ReadInfo(
        read_info_buffer_.get(),
        base::BindOnce(&AppCacheResponseTest::OnReadNonExistentInfoComplete,
                       base::Unretained(this), std::move(done_callback)));
    EXPECT_TRUE(reader_->IsReadPending());
    expected_read_result_ = net::ERR_CACHE_MISS;
  }

  void OnReadNonExistentInfoComplete(base::OnceClosure done_callback,
                                     int result) {
    EXPECT_FALSE(reader_->IsReadPending());
    EXPECT_EQ(expected_read_result_, result);
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheResponseTest::ReadNonExistentData,
                       base::Unretained(this), std::move(done_callback)));
  }

  void ReadNonExistentData(base::OnceClosure callback) {
    EXPECT_FALSE(reader_->IsReadPending());
    read_buffer_ = base::MakeRefCounted<IOBuffer>(kBlockSize);
    reader_->ReadData(
        read_buffer_.get(), kBlockSize,
        base::BindOnce(&AppCacheResponseTest::OnReadComplete,
                       base::Unretained(this), CallbackMode::kPostTask,
                       std::move(callback)));
    EXPECT_TRUE(reader_->IsReadPending());
    expected_read_result_ = net::ERR_CACHE_MISS;
  }

  // LoadResponseInfo_Miss ----------------------------------------------------
  void LoadResponseInfo_Miss() {
    base::RunLoop run_loop;
    MockStorageDelegate storage_delegate(this, run_loop.QuitClosure());
    service_->storage()->LoadResponseInfo(GURL(), kNoSuchResponseId,
                                          &storage_delegate);
    run_loop.Run();

    LoadResponseInfo_Miss_Verify(storage_delegate);
  }

  void LoadResponseInfo_Miss_Verify(MockStorageDelegate& storage_delegate) {
    EXPECT_EQ(kNoSuchResponseId, storage_delegate.loaded_info_id_);
    EXPECT_TRUE(!storage_delegate.loaded_info_.get());
  }

  // LoadResponseInfo_Hit ----------------------------------------------------
  void LoadResponseInfo_Hit() {
    // This tests involves multiple async steps.
    // 1. Write a response headers and body to storage
    //   a. headers
    //   b. body
    // 2. Use LoadResponseInfo to read the response headers back out
    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();

    base::RunLoop run_loop;
    MockStorageDelegate storage_delegate(this, run_loop.QuitClosure());
    WriteBasicResponse(base::BindLambdaForTesting([&]() {
      writer_.reset();
      service_->storage()->LoadResponseInfo(GURL(), written_response_id_,
                                            &storage_delegate);
    }));
    run_loop.Run();

    LoadResponseInfo_Hit_Verify(storage_delegate);
  }

  void LoadResponseInfo_Hit_Verify(MockStorageDelegate& storage_delegate) {
    EXPECT_EQ(written_response_id_, storage_delegate.loaded_info_id_);
    EXPECT_TRUE(storage_delegate.loaded_info_.get());
    EXPECT_TRUE(CompareHttpResponseInfos(
        *write_info_buffer_->http_info,
        storage_delegate.loaded_info_->http_response_info()));
    EXPECT_EQ(basic_response_size(),
              storage_delegate.loaded_info_->response_data_size());
  }

  // Metadata -------------------------------------------------
  void Metadata() {
    // This tests involves multiple async steps.
    // 1. Write a response headers and body to storage
    //   a. headers
    //   b. body
    // 2. Write metadata "Metadata First" using AppCacheResponseMetadataWriter.
    // 3. Check metadata was written.
    // 4. Write metadata "Second".
    // 5. Check metadata was written and was truncated .
    // 6. Write metadata "".
    // 7. Check metadata was deleted.

    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();

    {
      base::RunLoop run_loop;
      WriteBasicResponse(
          base::BindOnce(&AppCacheResponseTest::Metadata_ResetWriter,
                         base::Unretained(this), run_loop.QuitClosure()));
      run_loop.Run();
    }

    Metadata_WriteLoadVerify("Metadata first");
    Metadata_WriteLoadVerify("Second");
    Metadata_WriteLoadVerify("");
  }

  void Metadata_ResetWriter(base::OnceClosure done_callback) {
    writer_.reset();
    std::move(done_callback).Run();
  }

  void Metadata_WriteLoadVerify(const char* metadata) {
    base::RunLoop run_loop;
    MockStorageDelegate storage_delegate(this, run_loop.QuitClosure());

    Metadata_WriteMetadata("Metadata First", base::BindLambdaForTesting([&]() {
                             metadata_writer_.reset();
                             service_->storage()->LoadResponseInfo(
                                 GURL(), written_response_id_,
                                 &storage_delegate);
                           }));
    run_loop.Run();
  }

  void Metadata_WriteMetadata(const char* metadata,
                              base::OnceClosure callback) {
    metadata_writer_ =
        service_->storage()->CreateResponseMetadataWriter(written_response_id_);
    scoped_refptr<IOBuffer> buffer =
        base::MakeRefCounted<WrappedIOBuffer>(metadata);
    WriteResponseMetadata(buffer.get(), strlen(metadata), std::move(callback));
  }

  void Metadata_VerifyMetadata(const char* metadata,
                               MockStorageDelegate& storage_delegate) {
    EXPECT_EQ(written_response_id_, storage_delegate.loaded_info_id_);
    EXPECT_TRUE(storage_delegate.loaded_info_.get());
    EXPECT_TRUE(CompareHttpResponseInfos(
        *write_info_buffer_->http_info,
        storage_delegate.loaded_info_->http_response_info()));
    EXPECT_EQ(basic_response_size(),
              storage_delegate.loaded_info_->response_data_size());
  }

  // AmountWritten ----------------------------------------------------

  void AmountWritten() {
    static const char kHttpHeaders[] = "HTTP/1.0 200 OK\0\0";
    std::string raw_headers(kHttpHeaders, base::size(kHttpHeaders));
    std::unique_ptr<net::HttpResponseInfo> head =
        MakeHttpResponseInfo(raw_headers);
    int expected_amount_written =
        GetHttpResponseInfoSize(*head) + kNumBlocks * kBlockSize;

    // Build callbacks in reverse order.
    base::RunLoop run_loop;
    base::OnceClosure callback = run_loop.QuitClosure();
    for (int i = 0; i < kNumBlocks; ++i) {
      callback = base::BindOnce(&AppCacheResponseTest::WriteOneBlock,
                                base::Unretained(this), kNumBlocks - i,
                                CallbackMode::kPostTask, std::move(callback));
    }

    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&AppCacheResponseTest::WriteResponseHead,
                                  base::Unretained(this), std::move(head),
                                  std::move(callback)));
    run_loop.Run();

    Verify_AmountWritten(expected_amount_written);
  }

  void Verify_AmountWritten(int expected_amount_written) {
    EXPECT_EQ(expected_amount_written, writer_->amount_written());
  }

  // WriteThenVariouslyReadResponse -------------------------------------------

  void WriteThenVariouslyReadResponse() {
    // This tests involves multiple async steps.
    // 1. First, write a large body using multiple writes, we don't bother
    //    with a response head for this test.
    // 2. Read the entire body, using multiple reads
    // 3. Read the entire body, using one read.
    // 4. Attempt to read beyond the EOF.
    // 5. Read just a range.
    // 6. Attempt to read beyond EOF of a range.

    // Push tasks in reverse order
    base::RunLoop run_loop;
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheResponseTest::
                           WriteThenVariouslyReadResponse_WriteOutReadInBlocks,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void WriteThenVariouslyReadResponse_WriteOutReadInBlocks(
      base::OnceClosure done_callback) {
    WriteOutBlocks(base::BindOnce(
        &AppCacheResponseTest::ReadInBlocks, base::Unretained(this),
        base::BindOnce(&AppCacheResponseTest::ReadAllAtOnce,
                       base::Unretained(this), std::move(done_callback))));
  }

  void ReadAllAtOnce(base::OnceClosure done_callback) {
    reader_ =
        service_->storage()->CreateResponseReader(GURL(), written_response_id_);
    int big_size = kNumBlocks * kBlockSize;
    ReadResponseBody(
        base::MakeRefCounted<IOBuffer>(big_size), big_size,
        CallbackMode::kPostTask,
        base::BindOnce(&AppCacheResponseTest::VerifyAllAtOnce,
                       base::Unretained(this), std::move(done_callback)));
  }

  void VerifyAllAtOnce(base::OnceClosure done_callback) {
    char* p = read_buffer_->data();
    for (int i = 0; i < kNumBlocks; ++i, p += kBlockSize)
      EXPECT_TRUE(CheckData(i + 1, p, kBlockSize));
    ReadPastEOF(std::move(done_callback));
  }

  void ReadPastEOF(base::OnceClosure done_callback) {
    EXPECT_FALSE(reader_->IsReadPending());
    read_buffer_ = base::MakeRefCounted<IOBuffer>(kBlockSize);
    expected_read_result_ = 0;
    reader_->ReadData(
        read_buffer_.get(), kBlockSize,
        base::BindOnce(
            &AppCacheResponseTest::OnReadComplete, base::Unretained(this),
            CallbackMode::kPostTask,
            base::BindOnce(&AppCacheResponseTest::ReadRange,
                           base::Unretained(this), std::move(done_callback))));
  }

  void ReadRange(base::OnceClosure done_callback) {
    reader_ =
        service_->storage()->CreateResponseReader(GURL(), written_response_id_);
    reader_->SetReadRange(kBlockSize, kBlockSize);
    ReadResponseBody(
        base::MakeRefCounted<IOBuffer>(kBlockSize), kBlockSize,
        CallbackMode::kPostTask,
        base::BindOnce(
            &AppCacheResponseTest::VerifyRange, base::Unretained(this),
            base::BindOnce(&AppCacheResponseTest::ReadPastRangeEnd,
                           base::Unretained(this), std::move(done_callback))));
  }

  void VerifyRange(base::OnceClosure verify_callback) {
    EXPECT_TRUE(CheckData(2, read_buffer_->data(), kBlockSize));
    std::move(verify_callback).Run();
  }

  void ReadPastRangeEnd(base::OnceClosure done_callback) {
    EXPECT_FALSE(reader_->IsReadPending());
    read_buffer_ = base::MakeRefCounted<IOBuffer>(kBlockSize);
    expected_read_result_ = 0;
    reader_->ReadData(
        read_buffer_.get(), kBlockSize,
        base::BindOnce(
            &AppCacheResponseTest::OnReadComplete, base::Unretained(this),
            CallbackMode::kPostTask,
            base::BindOnce(&AppCacheResponseTest::ReadRangePartiallyBeyondEOF,
                           base::Unretained(this), std::move(done_callback))));
  }

  void ReadRangePartiallyBeyondEOF(base::OnceClosure done_callback) {
    reader_ =
        service_->storage()->CreateResponseReader(GURL(), written_response_id_);
    reader_->SetReadRange(kBlockSize, kNumBlocks * kBlockSize);
    ReadResponseBody(
        base::MakeRefCounted<IOBuffer>(kNumBlocks * kBlockSize),
        kNumBlocks * kBlockSize, CallbackMode::kPostTask,
        base::BindOnce(
            &AppCacheResponseTest::VerifyRangeBeyondEOF, base::Unretained(this),
            base::BindOnce(&AppCacheResponseTest::ReadRangeFullyBeyondEOF,
                           base::Unretained(this), std::move(done_callback))));
    expected_read_result_ = (kNumBlocks - 1) * kBlockSize;
  }

  void VerifyRangeBeyondEOF(base::OnceClosure verify_callback) {
    // Just verify the first 1k
    VerifyRange(std::move(verify_callback));
  }

  void ReadRangeFullyBeyondEOF(base::OnceClosure done_callback) {
    reader_ =
        service_->storage()->CreateResponseReader(GURL(), written_response_id_);
    reader_->SetReadRange((kNumBlocks * kBlockSize) + 1, kBlockSize);
    ReadResponseBody(base::MakeRefCounted<IOBuffer>(kBlockSize), kBlockSize,
                     CallbackMode::kPostTask, std::move(done_callback));
    expected_read_result_ = 0;
  }

  // IOChaining -------------------------------------------
  void IOChaining() {
    // 1. Write several blocks out initiating the subsequent write
    //    from within the completion callback of the previous write.
    // 2. Read and verify several blocks in similarly chaining reads.

    base::RunLoop run_loop;
    WriteOutBlocksImmediately(run_loop.QuitClosure());
    run_loop.Run();
  }

  void WriteOutBlocksImmediately(base::OnceClosure done_callback) {
    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();

    base::OnceClosure callback =
        base::BindOnce(&AppCacheResponseTest::ReadInBlocksImmediately,
                       base::Unretained(this), std::move(done_callback));
    for (int i = 0; i < kNumBlocks; ++i) {
      callback = base::BindOnce(&AppCacheResponseTest::WriteOneBlock,
                                base::Unretained(this), kNumBlocks - i,
                                CallbackMode::kImmediate, std::move(callback));
    }
    std::move(callback).Run();
  }

  void ReadInBlocksImmediately(base::OnceClosure done_callback) {
    writer_.reset();
    reader_ =
        service_->storage()->CreateResponseReader(GURL(), written_response_id_);

    base::OnceClosure callback = std::move(done_callback);
    for (int i = 0; i < kNumBlocks; ++i) {
      callback = base::BindOnce(&AppCacheResponseTest::ReadOneBlockImmediately,
                                base::Unretained(this), kNumBlocks - i,
                                std::move(callback));
    }
    std::move(callback).Run();
  }

  void ReadOneBlockImmediately(int block_number, base::OnceClosure callback) {
    ReadResponseBody(base::MakeRefCounted<IOBuffer>(kBlockSize), kBlockSize,
                     CallbackMode::kImmediate,
                     base::BindOnce(&AppCacheResponseTest::VerifyOneBlock,
                                    base::Unretained(this), block_number,
                                    std::move(callback)));
  }

  // DeleteWithinCallbacks -------------------------------------------
  void DeleteWithinCallbacks() {
    // 1. Write out a few blocks normally, and upon
    //    completion of the last write, delete the writer.
    // 2. Read in a few blocks normally, and upon completion
    //    of the last read, delete the reader.

    should_delete_reader_in_completion_callback_ = true;
    reader_deletion_count_down_ = kNumBlocks;
    should_delete_writer_in_completion_callback_ = true;
    writer_deletion_count_down_ = kNumBlocks;

    base::RunLoop run_loop;
    WriteOutBlocks(base::BindOnce(&AppCacheResponseTest::ReadInBlocks,
                                  base::Unretained(this),
                                  run_loop.QuitClosure()));
    run_loop.Run();
  }

  // DeleteWithIOPending -------------------------------------------
  void DeleteWithIOPending() {
    // 1. Write a few blocks normally.
    // 2. Start a write, delete with it pending.
    // 3. Start a read, delete with it pending.

    base::RunLoop run_loop;
    WriteOutBlocks(base::BindOnce(&AppCacheResponseTest::WriteThenDelete,
                                  base::Unretained(this),
                                  run_loop.QuitClosure()));
    run_loop.Run();
  }

  void WriteThenDelete(base::OnceClosure done_callback) {
    write_callback_was_called_ = false;
    WriteOneBlock(5, CallbackMode::kPostTask, base::DoNothing());
    EXPECT_TRUE(writer_->IsWritePending());
    writer_.reset();

    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheResponseTest::ReadThenDelete,
                       base::Unretained(this), std::move(done_callback)));
  }

  void ReadThenDelete(base::OnceClosure done_callback) {
    read_callback_was_called_ = false;
    reader_ =
        service_->storage()->CreateResponseReader(GURL(), written_response_id_);
    ReadResponseBody(base::MakeRefCounted<IOBuffer>(kBlockSize), kBlockSize,
                     CallbackMode::kImmediate, base::DoNothing());
    EXPECT_TRUE(reader_->IsReadPending());
    reader_.reset();

    // Wait a moment to verify no callbacks.
    GetIOThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AppCacheResponseTest::VerifyNoCallbacks,
                       base::Unretained(this), std::move(done_callback)),
        base::TimeDelta::FromMilliseconds(10));
  }

  void VerifyNoCallbacks(base::OnceClosure done_callback) {
    EXPECT_TRUE(!write_callback_was_called_);
    EXPECT_TRUE(!read_callback_was_called_);
    std::move(done_callback).Run();
  }

  // Data members

  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;

  std::unique_ptr<MockAppCacheService> service_;

  std::unique_ptr<AppCacheResponseReader> reader_;
  scoped_refptr<HttpResponseInfoIOBuffer> read_info_buffer_;
  scoped_refptr<IOBuffer> read_buffer_;
  int expected_read_result_ = 0;
  bool should_delete_reader_in_completion_callback_ = false;
  int reader_deletion_count_down_ = 0;
  bool read_callback_was_called_ = false;

  int64_t written_response_id_ = 0;
  std::unique_ptr<AppCacheResponseWriter> writer_;
  std::unique_ptr<AppCacheResponseMetadataWriter> metadata_writer_;
  scoped_refptr<HttpResponseInfoIOBuffer> write_info_buffer_;
  scoped_refptr<IOBuffer> write_buffer_;
  int expected_write_result_ = 0;
  bool should_delete_writer_in_completion_callback_ = false;
  int writer_deletion_count_down_ = 0;
  bool write_callback_was_called_ = false;
};

TEST_F(AppCacheResponseTest, ReadNonExistentResponse) {
  RunTestOnIOThread(&AppCacheResponseTest::ReadNonExistentResponse);
}

TEST_F(AppCacheResponseTest, LoadResponseInfo_Miss) {
  RunTestOnIOThread(&AppCacheResponseTest::LoadResponseInfo_Miss);
}

TEST_F(AppCacheResponseTest, LoadResponseInfo_Hit) {
  RunTestOnIOThread(&AppCacheResponseTest::LoadResponseInfo_Hit);
}

TEST_F(AppCacheResponseTest, Metadata) {
  RunTestOnIOThread(&AppCacheResponseTest::Metadata);
}

TEST_F(AppCacheResponseTest, AmountWritten) {
  RunTestOnIOThread(&AppCacheResponseTest::AmountWritten);
}

TEST_F(AppCacheResponseTest, WriteThenVariouslyReadResponse) {
  RunTestOnIOThread(&AppCacheResponseTest::WriteThenVariouslyReadResponse);
}

TEST_F(AppCacheResponseTest, IOChaining) {
  RunTestOnIOThread(&AppCacheResponseTest::IOChaining);
}

TEST_F(AppCacheResponseTest, DeleteWithinCallbacks) {
  RunTestOnIOThread(&AppCacheResponseTest::DeleteWithinCallbacks);
}

TEST_F(AppCacheResponseTest, DeleteWithIOPending) {
  RunTestOnIOThread(&AppCacheResponseTest::DeleteWithIOPending);
}

}  // namespace content
