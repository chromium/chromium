// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_FILE_ELEMENT_READER_H_
#define NET_BASE_UPLOAD_FILE_ELEMENT_READER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/upload_element_reader.h"

namespace base {
class TaskRunner;
}

namespace net {

class FileStream;

// An UploadElementReader implementation for file.
class NET_EXPORT UploadFileElementReader : public UploadElementReader {
 public:
  // |file| must be valid and opened for reading. On Windows, the file must have
  // been opened with File::FLAG_ASYNC, and elsewhere it must have ben opened
  // without it. |path| is never validated or used to re-open the file. It's
  // only used as the return value for path().
  // |task_runner| is used to perform file operations. It must not be NULL.
  //
  // TODO(mmenke): Remove |task_runner| argument, and use the ThreadPool
  // instead.
  UploadFileElementReader(base::TaskRunner* task_runner,
                          base::File file,
                          const base::FilePath& path,
                          uint64_t range_offset,
                          uint64_t range_length,
                          const base::Time& expected_modification_time);

  // Same a above, but takes a FilePath instead.
  // TODO(mmenke): Remove if all consumers can be switched to the first
  // constructor.
  UploadFileElementReader(base::TaskRunner* task_runner,
                          const base::FilePath& path,
                          uint64_t range_offset,
                          uint64_t range_length,
                          const base::Time& expected_modification_time);

  ~UploadFileElementReader() override;

  const base::FilePath& path() const { return path_; }
  uint64_t range_offset() const { return range_offset_; }
  uint64_t range_length() const { return range_length_; }
  const base::Time& expected_modification_time() const {
    return expected_modification_time_;
  }

  // UploadElementReader overrides:
  const UploadFileElementReader* AsFileReader() const override;
  int Init(CompletionOnceCallback callback) override;
  uint64_t GetContentLength() const override;
  uint64_t BytesRemaining() const override;
  int Read(IOBuffer* buf,
           int buf_length,
           CompletionOnceCallback callback) override;

 private:
  enum class State {
    // No async operation is pending.
    IDLE,

    // The ordered sequence of events started by calling Init().

    // Opens file. State is skipped if file already open.
    OPEN,
    OPEN_COMPLETE,
    SEEK,
    GET_FILE_INFO,
    GET_FILE_INFO_COMPLETE,

    // There is no READ state as reads are always started immediately on Read().
    READ_COMPLETE,
  };
  FRIEND_TEST_ALL_PREFIXES(ElementsUploadDataStreamTest, FileSmallerThanLength);
  FRIEND_TEST_ALL_PREFIXES(HttpNetworkTransactionTest,
                           UploadFileSmallerThanLength);

  int DoLoop(int result);

  int DoOpen();
  int DoOpenComplete(int result);
  int DoSeek();
  int DoGetFileInfo(int result);
  int DoGetFileInfoComplete(int result);
  int DoReadComplete(int result);

  void OnIOComplete(int result);

  // Sets an value to override the result for GetContentLength().
  // Used for tests.
  struct NET_EXPORT_PRIVATE ScopedOverridingContentLengthForTests {
    explicit ScopedOverridingContentLengthForTests(uint64_t value);
    ~ScopedOverridingContentLengthForTests();
  };

  scoped_refptr<base::TaskRunner> task_runner_;
  const base::FilePath path_;
  const uint64_t range_offset_;
  const uint64_t range_length_;
  const base::Time expected_modification_time_;
  std::unique_ptr<FileStream> file_stream_;
  uint64_t content_length_;
  uint64_t bytes_remaining_;

  // File information. Only valid during GET_FILE_INFO_COMPLETE state.
  base::File::Info file_info_;

  State next_state_;
  CompletionOnceCallback pending_callback_;
  // True if Init() was called while an async operation was in progress.
  bool init_called_while_operation_pending_;

  base::WeakPtrFactory<UploadFileElementReader> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UploadFileElementReader);
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_FILE_ELEMENT_READER_H_
