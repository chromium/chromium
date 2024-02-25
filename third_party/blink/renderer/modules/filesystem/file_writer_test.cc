// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/filesystem/file_writer_base.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

// We use particular offsets to trigger particular behaviors
// in the TestableFileWriter.
const int kNoOffset = -1;
const int kBasicFileTruncate_Offset = 1;
const int kErrorFileTruncate_Offset = 2;
const int kCancelFileTruncate_Offset = 3;
const int kCancelFailedTruncate_Offset = 4;
const int kBasicFileWrite_Offset = 1;
const int kErrorFileWrite_Offset = 2;
const int kMultiFileWrite_Offset = 3;
const int kCancelFileWriteBeforeCompletion_Offset = 4;
const int kCancelFileWriteAfterCompletion_Offset = 5;

KURL mock_path_as_kurl() {
  return KURL("MockPath");
}

Blob* CreateTestBlob() {
  return MakeGarbageCollected<Blob>(BlobDataHandle::Create());
}

}  // namespace

class TestableFileWriter : public GarbageCollected<TestableFileWriter>,
                           public FileWriterBase {
 public:
  explicit TestableFileWriter() { reset(); }

  void reset() {
    received_truncate_ = false;
    received_truncate_path_ = KURL();
    received_truncate_offset_ = kNoOffset;
    received_write_ = false;
    received_write_path_ = KURL();
    received_write_offset_ = kNoOffset;
    received_write_blob_ = nullptr;
    received_cancel_ = false;

    received_did_write_count_ = 0;
    received_did_write_bytes_total_ = 0;
    received_did_write_complete_ = false;
    received_did_truncate_ = false;
    received_did_fail_ = false;
    fail_error_received_ = static_cast<base::File::Error>(0);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(received_write_blob_);
    FileWriterBase::Trace(visitor);
  }

  bool received_truncate_;
  KURL received_truncate_path_;
  int64_t received_truncate_offset_;
  bool received_write_;
  KURL received_write_path_;
  Member<const Blob> received_write_blob_;
  int64_t received_write_offset_;
  bool received_cancel_;

  int received_did_write_count_;
  int64_t received_did_write_bytes_total_;
  bool received_did_write_complete_;
  bool received_did_truncate_;
  bool received_did_fail_;
  base::File::Error fail_error_received_;

 protected:
  void DoTruncate(const KURL& path, int64_t offset) override {
    received_truncate_ = true;
    received_truncate_path_ = path;
    received_truncate_offset_ = offset;

    if (offset == kBasicFileTruncate_Offset) {
      DidSucceed();
    } else if (offset == kErrorFileTruncate_Offset) {
      DidFail(base::File::FILE_ERROR_NOT_FOUND);
    } else if (offset == kCancelFileTruncate_Offset) {
      Cancel();
      DidSucceed();  // truncate completion
      DidSucceed();  // cancel completion
    } else if (offset == kCancelFailedTruncate_Offset) {
      Cancel();
      DidFail(base::File::FILE_ERROR_NOT_FOUND);  // truncate completion
      DidSucceed();                               // cancel completion
    } else {
      FAIL();
    }
  }

  void DoWrite(const KURL& path, const Blob& blob, int64_t offset) override {
    received_write_ = true;
    received_write_path_ = path;
    received_write_offset_ = offset;
    received_write_blob_ = &blob;

    if (offset == kBasicFileWrite_Offset) {
      DidWrite(1, true);
    } else if (offset == kErrorFileWrite_Offset) {
      DidFail(base::File::FILE_ERROR_NOT_FOUND);
    } else if (offset == kMultiFileWrite_Offset) {
      DidWrite(1, false);
      DidWrite(1, false);
      DidWrite(1, true);
    } else if (offset == kCancelFileWriteBeforeCompletion_Offset) {
      DidWrite(1, false);
      Cancel();
      DidWrite(1, false);
      DidWrite(1, false);
      DidFail(base::File::FILE_ERROR_NOT_FOUND);  // write completion
      DidSucceed();                               // cancel completion
    } else if (offset == kCancelFileWriteAfterCompletion_Offset) {
      DidWrite(1, false);
      Cancel();
      DidWrite(1, false);
      DidWrite(1, false);
      DidWrite(1, true);                          // write completion
      DidFail(base::File::FILE_ERROR_NOT_FOUND);  // cancel completion
    } else {
      FAIL();
    }
  }

  void DoCancel() override { received_cancel_ = true; }

  void DidWriteImpl(int64_t bytes, bool complete) override {
    EXPECT_FALSE(received_did_write_complete_);
    ++received_did_write_count_;
    received_did_write_bytes_total_ += bytes;
    if (complete)
      received_did_write_complete_ = true;
  }

  void DidTruncateImpl() override {
    EXPECT_FALSE(received_did_truncate_);
    received_did_truncate_ = true;
  }

  void DidFailImpl(base::File::Error error) override {
    EXPECT_FALSE(received_did_fail_);
    received_did_fail_ = true;
    fail_error_received_ = error;
  }
};

class FileWriterTest : public testing::Test {
 public:
  FileWriterTest() = default;

  FileWriterTest(const FileWriterTest&) = delete;
  FileWriterTest& operator=(const FileWriterTest&) = delete;

  FileWriterBase* writer() { return testable_writer_.Get(); }

 protected:
  void SetUp() override {
    testable_writer_ = MakeGarbageCollected<TestableFileWriter>();
    testable_writer_->Initialize(mock_path_as_kurl(), 10);
  }

  test::TaskEnvironment task_environment_;
  Persistent<TestableFileWriter> testable_writer_;
};

TEST_F(FileWriterTest, BasicFileWrite) {
  Blob* blob = CreateTestBlob();
  writer()->Write(kBasicFileWrite_Offset, *blob);

  // Check that the Do* methods of the derived class get called correctly.
  EXPECT_TRUE(testable_writer_->received_write_);
  EXPECT_EQ(testable_writer_->received_write_path_, mock_path_as_kurl());
  EXPECT_EQ(kBasicFileWrite_Offset, testable_writer_->received_write_offset_);
  EXPECT_EQ(blob, testable_writer_->received_write_blob_);
  EXPECT_FALSE(testable_writer_->received_truncate_);
  EXPECT_FALSE(testable_writer_->received_cancel_);

  // Check that the Did*Impl methods of the client gets called correctly.
  EXPECT_EQ(1, testable_writer_->received_did_write_count_);
  EXPECT_TRUE(testable_writer_->received_did_write_complete_);
  EXPECT_EQ(1, testable_writer_->received_did_write_bytes_total_);
  EXPECT_FALSE(testable_writer_->received_did_truncate_);
  EXPECT_FALSE(testable_writer_->received_did_fail_);
}

TEST_F(FileWriterTest, BasicFileTruncate) {
  writer()->Truncate(kBasicFileTruncate_Offset);

  // Check that the Do* methods of the derived class get called correctly.
  EXPECT_TRUE(testable_writer_->received_truncate_);
  EXPECT_EQ(mock_path_as_kurl(), testable_writer_->received_truncate_path_);
  EXPECT_EQ(kBasicFileTruncate_Offset,
            testable_writer_->received_truncate_offset_);
  EXPECT_FALSE(testable_writer_->received_write_);
  EXPECT_FALSE(testable_writer_->received_cancel_);

  // Check that the Did*Impl methods of the client gets called correctly.
  EXPECT_TRUE(testable_writer_->received_did_truncate_);
  EXPECT_EQ(0, testable_writer_->received_did_write_count_);
  EXPECT_FALSE(testable_writer_->received_did_fail_);
}

TEST_F(FileWriterTest, ErrorFileWrite) {
  Blob* blob = CreateTestBlob();
  writer()->Write(kErrorFileWrite_Offset, *blob);

  // Check that the Do* methods of the derived class get called correctly.
  EXPECT_TRUE(testable_writer_->received_write_);
  EXPECT_EQ(testable_writer_->received_write_path_, mock_path_as_kurl());
  EXPECT_EQ(kErrorFileWrite_Offset, testable_writer_->received_write_offset_);
  EXPECT_EQ(blob, testable_writer_->received_write_blob_);
  EXPECT_FALSE(testable_writer_->received_truncate_);
  EXPECT_FALSE(testable_writer_->received_cancel_);

  // Check that the Did*Impl methods of the client gets called correctly.
  EXPECT_TRUE(testable_writer_->received_did_fail_);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            testable_writer_->fail_error_received_);
  EXPECT_EQ(0, testable_writer_->received_did_write_count_);
  EXPECT_FALSE(testable_writer_->received_did_truncate_);
}

TEST_F(FileWriterTest, ErrorFileTruncate) {
  writer()->Truncate(kErrorFileTruncate_Offset);

  // Check that the Do* methods of the derived class get called correctly.
  EXPECT_TRUE(testable_writer_->received_truncate_);
  EXPECT_EQ(mock_path_as_kurl(), testable_writer_->received_truncate_path_);
  EXPECT_EQ(kErrorFileTruncate_Offset,
            testable_writer_->received_truncate_offset_);
  EXPECT_FALSE(testable_writer_->received_write_);
  EXPECT_FALSE(testable_writer_->received_cancel_);

  // Check that the Did*Impl methods of the client gets called correctly.
  EXPECT_TRUE(testable_writer_->received_did_fail_);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            testable_writer_->fail_error_received_);
  EXPECT_FALSE(testable_writer_->received_did_truncate_);
  EXPECT_EQ(0, testable_writer_->received_did_write_count_);
}

TEST_F(FileWriterTest, MultiFileWrite) {
  Blob* blob = CreateTestBlob();
  writer()->Write(kMultiFileWrite_Offset, *blob);

  // Check that the Do* methods of the derived class get called correctly.
  EXPECT_TRUE(testable_writer_->received_write_);
  EXPECT_EQ(testable_writer_->received_write_path_, mock_path_as_kurl());
  EXPECT_EQ(kMultiFileWrite_Offset, testable_writer_->received_write_offset_);
  EXPECT_EQ(blob, testable_writer_->received_write_blob_);
  EXPECT_FALSE(testable_writer_->received_truncate_);
  EXPECT_FALSE(testable_writer_->received_cancel_);

  // Check that the Did*Impl methods of the client gets called correctly.
  EXPECT_EQ(3, testable_writer_->received_did_write_count_);
  EXPECT_TRUE(testable_writer_->received_did_write_complete_);
  EXPECT_EQ(3, testable_writer_->received_did_write_bytes_total_);
  EXPECT_FALSE(testable_writer_->received_did_truncate_);
  EXPECT_FALSE(testable_writer_->received_did_fail_);
}

TEST_F(FileWriterTest, CancelFileWriteBeforeCompletion) {
  Blob* blob = CreateTestBlob();
  writer()->Write(kCancelFileWriteBeforeCompletion_Offset, *blob);

  // Check that the Do* methods of the derived class get called correctly.
  EXPECT_TRUE(testable_writer_->received_write_);
  EXPECT_EQ(testable_writer_->received_write_path_, mock_path_as_kurl());
  EXPECT_EQ(kCancelFileWriteBeforeCompletion_Offset,
            testable_writer_->received_write_offset_);
  EXPECT_EQ(blob, testable_writer_->received_write_blob_);
  EXPECT_TRUE(testable_writer_->received_cancel_);
  EXPECT_FALSE(testable_writer_->received_truncate_);

  // Check that the Did*Impl methods of the client gets called correctly.
  EXPECT_TRUE(testable_writer_->received_did_fail_);
  EXPECT_EQ(base::File::FILE_ERROR_ABORT,
            testable_writer_->fail_error_received_);
  EXPECT_EQ(1, testable_writer_->received_did_write_count_);
  EXPECT_FALSE(testable_writer_->received_did_write_complete_);
  EXPECT_EQ(1, testable_writer_->received_did_write_bytes_total_);
  EXPECT_FALSE(testable_writer_->received_did_truncate_);
}

TEST_F(FileWriterTest, CancelFileWriteAfterCompletion) {
  Blob* blob = CreateTestBlob();
  writer()->Write(kCancelFileWriteAfterCompletion_Offset, *blob);

  // Check that the Do* methods of the derived class get called correctly.
  EXPECT_TRUE(testable_writer_->received_write_);
  EXPECT_EQ(testable_writer_->received_write_path_, mock_path_as_kurl());
  EXPECT_EQ(kCancelFileWriteAfterCompletion_Offset,
            testable_writer_->received_write_offset_);
  EXPECT_EQ(blob, testable_writer_->received_write_blob_);
  EXPECT_TRUE(testable_writer_->received_cancel_);
  EXPECT_FALSE(testable_writer_->received_truncate_);

  // Check that the Did*Impl methods of the client gets called correctly.
  EXPECT_TRUE(testable_writer_->received_did_fail_);
  EXPECT_EQ(base::File::FILE_ERROR_ABORT,
            testable_writer_->fail_error_received_);
  EXPECT_EQ(1, testable_writer_->received_did_write_count_);
  EXPECT_FALSE(testable_writer_->received_did_write_complete_);
  EXPECT_EQ(1, testable_writer_->received_did_write_bytes_total_);
  EXPECT_FALSE(testable_writer_->received_did_truncate_);
}

TEST_F(FileWriterTest, CancelFileTruncate) {
  writer()->Truncate(kCancelFileTruncate_Offset);

  // Check that the Do* methods of the derived class get called correctly.
  EXPECT_TRUE(testable_writer_->received_truncate_);
  EXPECT_EQ(mock_path_as_kurl(), testable_writer_->received_truncate_path_);
  EXPECT_EQ(kCancelFileTruncate_Offset,
            testable_writer_->received_truncate_offset_);
  EXPECT_TRUE(testable_writer_->received_cancel_);
  EXPECT_FALSE(testable_writer_->received_write_);

  // Check that the Did*Impl methods of the client gets called correctly.
  EXPECT_TRUE(testable_writer_->received_did_fail_);
  EXPECT_EQ(base::File::FILE_ERROR_ABORT,
            testable_writer_->fail_error_received_);
  EXPECT_FALSE(testable_writer_->received_did_truncate_);
  EXPECT_EQ(0, testable_writer_->received_did_write_count_);
}

TEST_F(FileWriterTest, CancelFailedTruncate) {
  writer()->Truncate(kCancelFailedTruncate_Offset);

  // Check that the Do* methods of the derived class get called correctly.
  EXPECT_TRUE(testable_writer_->received_truncate_);
  EXPECT_EQ(mock_path_as_kurl(), testable_writer_->received_truncate_path_);
  EXPECT_EQ(kCancelFailedTruncate_Offset,
            testable_writer_->received_truncate_offset_);
  EXPECT_TRUE(testable_writer_->received_cancel_);
  EXPECT_FALSE(testable_writer_->received_write_);

  // Check that the Did*Impl methods of the client gets called correctly.
  EXPECT_TRUE(testable_writer_->received_did_fail_);
  EXPECT_EQ(base::File::FILE_ERROR_ABORT,
            testable_writer_->fail_error_received_);
  EXPECT_FALSE(testable_writer_->received_did_truncate_);
  EXPECT_EQ(0, testable_writer_->received_did_write_count_);
}

}  // namespace blink
