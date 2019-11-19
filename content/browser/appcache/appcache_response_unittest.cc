// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/stack.h"
#include "base/location.h"
#include "base/message_loop/message_pump_type.h"
#include "base/pickle.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::IOBuffer;
using net::WrappedIOBuffer;

namespace content {

static const int kNumBlocks = 4;
static const int kBlockSize = 1024;
static const int kNoSuchResponseId = 123;

class AppCacheResponseTest : public testing::Test {
 public:
  // Test Harness -------------------------------------------------------------

  // Helper class used to verify test results
  class MockStorageDelegate : public AppCacheStorage::Delegate {
   public:
    explicit MockStorageDelegate(AppCacheResponseTest* test)
        : loaded_info_id_(0), test_(test) {
    }

    void OnResponseInfoLoaded(AppCacheResponseInfo* info,
                              int64_t response_id) override {
      loaded_info_ = info;
      loaded_info_id_ = response_id;
      test_->ScheduleNextTask();
    }

    scoped_refptr<AppCacheResponseInfo> loaded_info_;
    int64_t loaded_info_id_;
    AppCacheResponseTest* test_;
  };

  // Helper callback to run a test on our io_thread. The io_thread is spun up
  // once and reused for all tests.
  template <class Method>
  void MethodWrapper(Method method) {
    SetUpTest();
    (this->*method)();
  }

  static void SetUpTestCase() {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>();
    io_thread_ = std::make_unique<base::Thread>("AppCacheResponseTest Thread");
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    io_thread_->StartWithOptions(options);
  }

  static void TearDownTestCase() {
    io_thread_.reset();
    task_environment_.reset();
  }

  AppCacheResponseTest() {}

  template <class Method>
  void RunTestOnIOThread(Method method) {
    test_finished_event_ = std::make_unique<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    io_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&AppCacheResponseTest::MethodWrapper<Method>,
                                  base::Unretained(this), method));
    test_finished_event_->Wait();
  }

  void SetUpTest() {
    DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
    DCHECK(task_stack_.empty());
    storage_delegate_ = std::make_unique<MockStorageDelegate>(this);
    service_ = std::make_unique<MockAppCacheService>();
    expected_read_result_ = 0;
    expected_write_result_ = 0;
    written_response_id_ = 0;
    should_delete_reader_in_completion_callback_ = false;
    should_delete_writer_in_completion_callback_ = false;
    reader_deletion_count_down_ = 0;
    writer_deletion_count_down_ = 0;
    read_callback_was_called_ = false;
    write_callback_was_called_ = false;
  }

  void TearDownTest() {
    DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
    while (!task_stack_.empty())
      task_stack_.pop();

    reader_.reset();
    read_buffer_ = nullptr;
    read_info_buffer_ = nullptr;
    writer_.reset();
    write_buffer_ = nullptr;
    write_info_buffer_ = nullptr;
    storage_delegate_.reset();
    service_.reset();
  }

  void TestFinished() {
    // We unwind the stack prior to finishing up to let stack
    // based objects get deleted.
    DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AppCacheResponseTest::TestFinishedUnwound,
                                  base::Unretained(this)));
  }

  void TestFinishedUnwound() {
    TearDownTest();
    test_finished_event_->Signal();
  }

  void PushNextTask(base::OnceClosure task) {
    task_stack_.push(
        std::pair<base::OnceClosure, bool>(std::move(task), false));
  }

  void PushNextTaskAsImmediate(base::OnceClosure task) {
    task_stack_.push(std::pair<base::OnceClosure, bool>(std::move(task), true));
  }

  void ScheduleNextTask() {
    DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
    if (task_stack_.empty()) {
      TestFinished();
      return;
    }
    base::OnceClosure task = std::move(task_stack_.top().first);
    bool immediate = task_stack_.top().second;
    task_stack_.pop();
    if (immediate)
      std::move(task).Run();
    else
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(task));
  }

  // Wrappers to call AppCacheResponseReader/Writer Read and Write methods

  void WriteBasicResponse() {
    static const char kHttpHeaders[] =
        "HTTP/1.0 200 OK\0Content-Length: 5\0\0";
    static const char kHttpBody[] = "Hello";
    scoped_refptr<IOBuffer> body =
        base::MakeRefCounted<WrappedIOBuffer>(kHttpBody);
    std::string raw_headers(kHttpHeaders, base::size(kHttpHeaders));
    WriteResponse(
        MakeHttpResponseInfo(raw_headers), body.get(), strlen(kHttpBody));
  }

  int basic_response_size() { return 5; }  // should match kHttpBody above

  void WriteResponse(std::unique_ptr<net::HttpResponseInfo> head,
                     IOBuffer* body,
                     int body_len) {
    DCHECK(body);
    scoped_refptr<IOBuffer> body_ref(body);
    PushNextTask(base::BindOnce(&AppCacheResponseTest::WriteResponseBody,
                                base::Unretained(this), body_ref, body_len));
    WriteResponseHead(std::move(head));
  }

  void WriteResponseHead(std::unique_ptr<net::HttpResponseInfo> head) {
    EXPECT_FALSE(writer_->IsWritePending());
    expected_write_result_ = GetHttpResponseInfoSize(*head);
    write_info_buffer_ =
        base::MakeRefCounted<HttpResponseInfoIOBuffer>(std::move(head));
    writer_->WriteInfo(
        write_info_buffer_.get(),
        base::BindOnce(&AppCacheResponseTest::OnWriteInfoComplete,
                       base::Unretained(this)));
  }

  void WriteResponseBody(scoped_refptr<IOBuffer> io_buffer, int buf_len) {
    EXPECT_FALSE(writer_->IsWritePending());
    write_buffer_ = std::move(io_buffer);
    expected_write_result_ = buf_len;
    writer_->WriteData(write_buffer_.get(), buf_len,
                       base::BindOnce(&AppCacheResponseTest::OnWriteComplete,
                                      base::Unretained(this)));
  }

  void WriteResponseMetadata(scoped_refptr<IOBuffer> io_buffer, int buf_len) {
    EXPECT_FALSE(metadata_writer_->IsWritePending());
    write_buffer_ = io_buffer;
    expected_write_result_ = buf_len;
    metadata_writer_->WriteMetadata(
        write_buffer_.get(), buf_len,
        base::BindOnce(&AppCacheResponseTest::OnMetadataWriteComplete,
                       base::Unretained(this)));
  }

  void ReadResponseBody(scoped_refptr<IOBuffer> io_buffer, int buf_len) {
    EXPECT_FALSE(reader_->IsReadPending());
    read_buffer_ = io_buffer;
    expected_read_result_ = buf_len;
    reader_->ReadData(read_buffer_.get(), buf_len,
                      base::BindOnce(&AppCacheResponseTest::OnReadComplete,
                                     base::Unretained(this)));
  }

  // AppCacheResponseReader / Writer completion callbacks

  void OnWriteInfoComplete(int result) {
    EXPECT_FALSE(writer_->IsWritePending());
    EXPECT_EQ(expected_write_result_, result);
    ScheduleNextTask();
  }

  void OnWriteComplete(int result) {
    EXPECT_FALSE(writer_->IsWritePending());
    write_callback_was_called_ = true;
    EXPECT_EQ(expected_write_result_, result);
    if (should_delete_writer_in_completion_callback_ &&
        --writer_deletion_count_down_ == 0) {
      writer_.reset();
    }
    ScheduleNextTask();
  }

  void OnMetadataWriteComplete(int result) {
    EXPECT_FALSE(metadata_writer_->IsWritePending());
    EXPECT_EQ(expected_write_result_, result);
    ScheduleNextTask();
  }

  void OnReadInfoComplete(int result) {
    EXPECT_FALSE(reader_->IsReadPending());
    EXPECT_EQ(expected_read_result_, result);
    ScheduleNextTask();
  }

  void OnReadComplete(int result) {
    EXPECT_FALSE(reader_->IsReadPending());
    read_callback_was_called_ = true;
    EXPECT_EQ(expected_read_result_, result);
    if (should_delete_reader_in_completion_callback_ &&
        --reader_deletion_count_down_ == 0) {
      reader_.reset();
    }
    ScheduleNextTask();
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

    // Push tasks in reverse order
    PushNextTask(base::BindOnce(&AppCacheResponseTest::ReadNonExistentData,
                                base::Unretained(this)));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::ReadNonExistentInfo,
                                base::Unretained(this)));
    ScheduleNextTask();
  }

  void ReadNonExistentInfo() {
    EXPECT_FALSE(reader_->IsReadPending());
    read_info_buffer_ = base::MakeRefCounted<HttpResponseInfoIOBuffer>();
    reader_->ReadInfo(read_info_buffer_.get(),
                      base::BindOnce(&AppCacheResponseTest::OnReadInfoComplete,
                                     base::Unretained(this)));
    EXPECT_TRUE(reader_->IsReadPending());
    expected_read_result_ = net::ERR_CACHE_MISS;
  }

  void ReadNonExistentData() {
    EXPECT_FALSE(reader_->IsReadPending());
    read_buffer_ = base::MakeRefCounted<IOBuffer>(kBlockSize);
    reader_->ReadData(read_buffer_.get(), kBlockSize,
                      base::BindOnce(&AppCacheResponseTest::OnReadComplete,
                                     base::Unretained(this)));
    EXPECT_TRUE(reader_->IsReadPending());
    expected_read_result_ = net::ERR_CACHE_MISS;
  }

  // LoadResponseInfo_Miss ----------------------------------------------------
  void LoadResponseInfo_Miss() {
    PushNextTask(
        base::BindOnce(&AppCacheResponseTest::LoadResponseInfo_Miss_Verify,
                       base::Unretained(this)));
    service_->storage()->LoadResponseInfo(GURL(), kNoSuchResponseId,
                                          storage_delegate_.get());
  }

  void LoadResponseInfo_Miss_Verify() {
    EXPECT_EQ(kNoSuchResponseId, storage_delegate_->loaded_info_id_);
    EXPECT_TRUE(!storage_delegate_->loaded_info_.get());
    TestFinished();
  }

  // LoadResponseInfo_Hit ----------------------------------------------------
  void LoadResponseInfo_Hit() {
    // This tests involves multiple async steps.
    // 1. Write a response headers and body to storage
    //   a. headers
    //   b. body
    // 2. Use LoadResponseInfo to read the response headers back out
    PushNextTask(
        base::BindOnce(&AppCacheResponseTest::LoadResponseInfo_Hit_Step2,
                       base::Unretained(this)));
    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();
    WriteBasicResponse();
  }

  void LoadResponseInfo_Hit_Step2() {
    writer_.reset();
    PushNextTask(
        base::BindOnce(&AppCacheResponseTest::LoadResponseInfo_Hit_Verify,
                       base::Unretained(this)));
    service_->storage()->LoadResponseInfo(GURL(), written_response_id_,
                                          storage_delegate_.get());
  }

  void LoadResponseInfo_Hit_Verify() {
    EXPECT_EQ(written_response_id_, storage_delegate_->loaded_info_id_);
    EXPECT_TRUE(storage_delegate_->loaded_info_.get());
    EXPECT_TRUE(CompareHttpResponseInfos(
        *write_info_buffer_->http_info,
        storage_delegate_->loaded_info_->http_response_info()));
    EXPECT_EQ(basic_response_size(),
              storage_delegate_->loaded_info_->response_data_size());
    TestFinished();
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

    // Push tasks in reverse order.
    PushNextTask(base::BindOnce(&AppCacheResponseTest::Metadata_VerifyMetadata,
                                base::Unretained(this), ""));
    PushNextTask(
        base::BindOnce(&AppCacheResponseTest::Metadata_LoadResponseInfo,
                       base::Unretained(this)));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::Metadata_WriteMetadata,
                                base::Unretained(this), ""));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::Metadata_VerifyMetadata,
                                base::Unretained(this), "Second"));
    PushNextTask(
        base::BindOnce(&AppCacheResponseTest::Metadata_LoadResponseInfo,
                       base::Unretained(this)));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::Metadata_WriteMetadata,
                                base::Unretained(this), "Second"));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::Metadata_VerifyMetadata,
                                base::Unretained(this), "Metadata First"));
    PushNextTask(
        base::BindOnce(&AppCacheResponseTest::Metadata_LoadResponseInfo,
                       base::Unretained(this)));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::Metadata_WriteMetadata,
                                base::Unretained(this), "Metadata First"));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::Metadata_ResetWriter,
                                base::Unretained(this)));
    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();
    WriteBasicResponse();
  }

  void Metadata_ResetWriter() {
    writer_.reset();
    ScheduleNextTask();
  }

  void Metadata_WriteMetadata(const char* metadata) {
    metadata_writer_ =
        service_->storage()->CreateResponseMetadataWriter(written_response_id_);
    scoped_refptr<IOBuffer> buffer =
        base::MakeRefCounted<WrappedIOBuffer>(metadata);
    WriteResponseMetadata(buffer.get(), strlen(metadata));
  }

  void Metadata_LoadResponseInfo() {
    metadata_writer_.reset();
    storage_delegate_ = std::make_unique<MockStorageDelegate>(this);
    service_->storage()->LoadResponseInfo(GURL(), written_response_id_,
                                          storage_delegate_.get());
  }

  void Metadata_VerifyMetadata(const char* metadata) {
    EXPECT_EQ(written_response_id_, storage_delegate_->loaded_info_id_);
    EXPECT_TRUE(storage_delegate_->loaded_info_.get());
    const net::HttpResponseInfo& read_head =
        storage_delegate_->loaded_info_->http_response_info();
    const int metadata_size = strlen(metadata);
    if (metadata_size) {
      EXPECT_TRUE(read_head.metadata.get());
      EXPECT_EQ(metadata_size, read_head.metadata->size());
      EXPECT_EQ(0, memcmp(metadata, read_head.metadata->data(), metadata_size));
    } else {
      EXPECT_FALSE(read_head.metadata.get());
    }
    EXPECT_TRUE(CompareHttpResponseInfos(
        *write_info_buffer_->http_info,
        storage_delegate_->loaded_info_->http_response_info()));
    EXPECT_EQ(basic_response_size(),
              storage_delegate_->loaded_info_->response_data_size());
    ScheduleNextTask();
  }

  // AmountWritten ----------------------------------------------------

  void AmountWritten() {
    static const char kHttpHeaders[] = "HTTP/1.0 200 OK\0\0";
    std::string raw_headers(kHttpHeaders, base::size(kHttpHeaders));
    std::unique_ptr<net::HttpResponseInfo> head =
        MakeHttpResponseInfo(raw_headers);
    int expected_amount_written =
        GetHttpResponseInfoSize(*head) + kNumBlocks * kBlockSize;

    // Push tasks in reverse order.
    PushNextTask(base::BindOnce(&AppCacheResponseTest::Verify_AmountWritten,
                                base::Unretained(this),
                                expected_amount_written));
    for (int i = 0; i < kNumBlocks; ++i) {
      PushNextTask(base::BindOnce(&AppCacheResponseTest::WriteOneBlock,
                                  base::Unretained(this), kNumBlocks - i));
    }
    PushNextTask(base::BindOnce(&AppCacheResponseTest::WriteResponseHead,
                                base::Unretained(this), std::move(head)));

    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();
    ScheduleNextTask();
  }

  void Verify_AmountWritten(int expected_amount_written) {
    EXPECT_EQ(expected_amount_written, writer_->amount_written());
    TestFinished();
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
    PushNextTask(base::BindOnce(&AppCacheResponseTest::ReadRangeFullyBeyondEOF,
                                base::Unretained(this)));
    PushNextTask(
        base::BindOnce(&AppCacheResponseTest::ReadRangePartiallyBeyondEOF,
                       base::Unretained(this)));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::ReadPastEOF,
                                base::Unretained(this)));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::ReadRange,
                                base::Unretained(this)));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::ReadPastEOF,
                                base::Unretained(this)));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::ReadAllAtOnce,
                                base::Unretained(this)));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::ReadInBlocks,
                                base::Unretained(this)));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::WriteOutBlocks,
                                base::Unretained(this)));

    // Get them going.
    ScheduleNextTask();
  }

  void WriteOutBlocks() {
    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();
    for (int i = 0; i < kNumBlocks; ++i) {
      PushNextTask(base::BindOnce(&AppCacheResponseTest::WriteOneBlock,
                                  base::Unretained(this), kNumBlocks - i));
    }
    ScheduleNextTask();
  }

  void WriteOneBlock(int block_number) {
    scoped_refptr<IOBuffer> io_buffer =
        base::MakeRefCounted<IOBuffer>(kBlockSize);
    FillData(block_number, io_buffer->data(), kBlockSize);
    WriteResponseBody(io_buffer, kBlockSize);
  }

  void ReadInBlocks() {
    writer_.reset();
    reader_ =
        service_->storage()->CreateResponseReader(GURL(), written_response_id_);
    for (int i = 0; i < kNumBlocks; ++i) {
      PushNextTask(base::BindOnce(&AppCacheResponseTest::ReadOneBlock,
                                  base::Unretained(this), kNumBlocks - i));
    }
    ScheduleNextTask();
  }

  void ReadOneBlock(int block_number) {
    PushNextTask(base::BindOnce(&AppCacheResponseTest::VerifyOneBlock,
                                base::Unretained(this), block_number));
    ReadResponseBody(base::MakeRefCounted<IOBuffer>(kBlockSize), kBlockSize);
  }

  void VerifyOneBlock(int block_number) {
    EXPECT_TRUE(CheckData(block_number, read_buffer_->data(), kBlockSize));
    ScheduleNextTask();
  }

  void ReadAllAtOnce() {
    PushNextTask(base::BindOnce(&AppCacheResponseTest::VerifyAllAtOnce,
                                base::Unretained(this)));
    reader_ =
        service_->storage()->CreateResponseReader(GURL(), written_response_id_);
    int big_size = kNumBlocks * kBlockSize;
    ReadResponseBody(base::MakeRefCounted<IOBuffer>(big_size), big_size);
  }

  void VerifyAllAtOnce() {
    char* p = read_buffer_->data();
    for (int i = 0; i < kNumBlocks; ++i, p += kBlockSize)
      EXPECT_TRUE(CheckData(i + 1, p, kBlockSize));
    ScheduleNextTask();
  }

  void ReadPastEOF() {
    EXPECT_FALSE(reader_->IsReadPending());
    read_buffer_ = base::MakeRefCounted<IOBuffer>(kBlockSize);
    expected_read_result_ = 0;
    reader_->ReadData(read_buffer_.get(), kBlockSize,
                      base::BindOnce(&AppCacheResponseTest::OnReadComplete,
                                     base::Unretained(this)));
  }

  void ReadRange() {
    PushNextTask(base::BindOnce(&AppCacheResponseTest::VerifyRange,
                                base::Unretained(this)));
    reader_ =
        service_->storage()->CreateResponseReader(GURL(), written_response_id_);
    reader_->SetReadRange(kBlockSize, kBlockSize);
    ReadResponseBody(base::MakeRefCounted<IOBuffer>(kBlockSize), kBlockSize);
  }

  void VerifyRange() {
    EXPECT_TRUE(CheckData(2, read_buffer_->data(), kBlockSize));
    ScheduleNextTask();  // ReadPastEOF is scheduled next
  }

  void ReadRangePartiallyBeyondEOF() {
    PushNextTask(base::BindOnce(&AppCacheResponseTest::VerifyRangeBeyondEOF,
                                base::Unretained(this)));
    reader_ =
        service_->storage()->CreateResponseReader(GURL(), written_response_id_);
    reader_->SetReadRange(kBlockSize, kNumBlocks * kBlockSize);
    ReadResponseBody(base::MakeRefCounted<IOBuffer>(kNumBlocks * kBlockSize),
                     kNumBlocks * kBlockSize);
    expected_read_result_ = (kNumBlocks - 1) * kBlockSize;
  }

  void VerifyRangeBeyondEOF() {
    // Just verify the first 1k
    VerifyRange();
  }

  void ReadRangeFullyBeyondEOF() {
    reader_ =
        service_->storage()->CreateResponseReader(GURL(), written_response_id_);
    reader_->SetReadRange((kNumBlocks * kBlockSize) + 1, kBlockSize);
    ReadResponseBody(base::MakeRefCounted<IOBuffer>(kBlockSize), kBlockSize);
    expected_read_result_ = 0;
  }

  // IOChaining -------------------------------------------
  void IOChaining() {
    // 1. Write several blocks out initiating the subsequent write
    //    from within the completion callback of the previous write.
    // 2. Read and verify several blocks in similarly chaining reads.

    // Push tasks in reverse order
    PushNextTaskAsImmediate(
        base::BindOnce(&AppCacheResponseTest::ReadInBlocksImmediately,
                       base::Unretained(this)));
    PushNextTaskAsImmediate(
        base::BindOnce(&AppCacheResponseTest::WriteOutBlocksImmediately,
                       base::Unretained(this)));

    // Get them going.
    ScheduleNextTask();
  }

  void WriteOutBlocksImmediately() {
    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();
    for (int i = 0; i < kNumBlocks; ++i) {
      PushNextTaskAsImmediate(
          base::BindOnce(&AppCacheResponseTest::WriteOneBlock,
                         base::Unretained(this), kNumBlocks - i));
    }
    ScheduleNextTask();
  }

  void ReadInBlocksImmediately() {
    writer_.reset();
    reader_ =
        service_->storage()->CreateResponseReader(GURL(), written_response_id_);
    for (int i = 0; i < kNumBlocks; ++i) {
      PushNextTaskAsImmediate(
          base::BindOnce(&AppCacheResponseTest::ReadOneBlockImmediately,
                         base::Unretained(this), kNumBlocks - i));
    }
    ScheduleNextTask();
  }

  void ReadOneBlockImmediately(int block_number) {
    PushNextTaskAsImmediate(
        base::BindOnce(&AppCacheResponseTest::VerifyOneBlock,
                       base::Unretained(this), block_number));
    ReadResponseBody(base::MakeRefCounted<IOBuffer>(kBlockSize), kBlockSize);
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

    PushNextTask(base::BindOnce(&AppCacheResponseTest::ReadInBlocks,
                                base::Unretained(this)));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::WriteOutBlocks,
                                base::Unretained(this)));
    ScheduleNextTask();
  }

  // DeleteWithIOPending -------------------------------------------
  void DeleteWithIOPending() {
    // 1. Write a few blocks normally.
    // 2. Start a write, delete with it pending.
    // 3. Start a read, delete with it pending.
    PushNextTask(base::BindOnce(&AppCacheResponseTest::ReadThenDelete,
                                base::Unretained(this)));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::WriteThenDelete,
                                base::Unretained(this)));
    PushNextTask(base::BindOnce(&AppCacheResponseTest::WriteOutBlocks,
                                base::Unretained(this)));
    ScheduleNextTask();
  }

  void WriteThenDelete() {
    write_callback_was_called_ = false;
    WriteOneBlock(5);
    EXPECT_TRUE(writer_->IsWritePending());
    writer_.reset();
    ScheduleNextTask();
  }

  void ReadThenDelete() {
    read_callback_was_called_ = false;
    reader_ =
        service_->storage()->CreateResponseReader(GURL(), written_response_id_);
    ReadResponseBody(base::MakeRefCounted<IOBuffer>(kBlockSize), kBlockSize);
    EXPECT_TRUE(reader_->IsReadPending());
    reader_.reset();

    // Wait a moment to verify no callbacks.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AppCacheResponseTest::VerifyNoCallbacks,
                       base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(10));
  }

  void VerifyNoCallbacks() {
    EXPECT_TRUE(!write_callback_was_called_);
    EXPECT_TRUE(!read_callback_was_called_);
    TestFinished();
  }

  // Data members

  std::unique_ptr<base::WaitableEvent> test_finished_event_;
  std::unique_ptr<MockStorageDelegate> storage_delegate_;
  std::unique_ptr<MockAppCacheService> service_;
  base::stack<std::pair<base::OnceClosure, bool>> task_stack_;

  std::unique_ptr<AppCacheResponseReader> reader_;
  scoped_refptr<HttpResponseInfoIOBuffer> read_info_buffer_;
  scoped_refptr<IOBuffer> read_buffer_;
  int expected_read_result_;
  bool should_delete_reader_in_completion_callback_;
  int reader_deletion_count_down_;
  bool read_callback_was_called_;

  int64_t written_response_id_;
  std::unique_ptr<AppCacheResponseWriter> writer_;
  std::unique_ptr<AppCacheResponseMetadataWriter> metadata_writer_;
  scoped_refptr<HttpResponseInfoIOBuffer> write_info_buffer_;
  scoped_refptr<IOBuffer> write_buffer_;
  int expected_write_result_;
  bool should_delete_writer_in_completion_callback_;
  int writer_deletion_count_down_;
  bool write_callback_was_called_;

  static std::unique_ptr<base::Thread> io_thread_;
  static std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

// static
std::unique_ptr<base::Thread> AppCacheResponseTest::io_thread_;
std::unique_ptr<base::test::TaskEnvironment>
    AppCacheResponseTest::task_environment_;

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
