// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_FETCHER_RESPONSE_WRITER_H_
#define NET_URL_REQUEST_URL_FETCHER_RESPONSE_WRITER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace net {

class FileStream;
class IOBuffer;
class URLFetcherFileWriter;
class URLFetcherStringWriter;

// This class encapsulates all state involved in writing URLFetcher response
// bytes to the destination.
class NET_EXPORT URLFetcherResponseWriter {
 public:
  virtual ~URLFetcherResponseWriter() {}

  // Initializes this instance. Returns an error code defined in
  // //net/base/net_errors.h. If ERR_IO_PENDING is returned, |callback| will be
  // run later with the result. If anything else is returned, |callback| will
  // *not* be called. Calling this method again after a Initialize() success
  // results in discarding already written data.
  virtual int Initialize(CompletionOnceCallback callback) = 0;

  // Writes |num_bytes| bytes in |buffer|, and returns the number of bytes
  // written or an error code defined in //net/base/net_errors.h. If
  // ERR_IO_PENDING is returned, |callback| will be run later with the result.
  // If anything else is returned, |callback| will *not* be called.
  virtual int Write(IOBuffer* buffer,
                    int num_bytes,
                    CompletionOnceCallback callback) = 0;

  // Finishes writing. If |net_error| is not OK, this method can be called
  // in the middle of another operation (eg. Initialize() and Write()). On
  // errors (|net_error| not OK), this method may be called before the previous
  // operation completed. In this case, URLFetcherResponseWriter may skip
  // graceful shutdown and completion of the pending operation. After such a
  // failure, the URLFetcherResponseWriter may be reused. Returns an error code
  // defined in //net/base/net_errors.h. If ERR_IO_PENDING is returned,
  // |callback| will be run later with the result. If anything else is returned,
  // |callback| will *not* be called.
  virtual int Finish(int net_error, CompletionOnceCallback callback) = 0;

  // Returns this instance's pointer as URLFetcherStringWriter when possible.
  virtual URLFetcherStringWriter* AsStringWriter();

  // Returns this instance's pointer as URLFetcherFileWriter when possible.
  virtual URLFetcherFileWriter* AsFileWriter();
};

// URLFetcherResponseWriter implementation for std::string.
class NET_EXPORT URLFetcherStringWriter : public URLFetcherResponseWriter {
 public:
  URLFetcherStringWriter();
  ~URLFetcherStringWriter() override;

  const std::string& data() const { return data_; }

  // URLFetcherResponseWriter overrides:
  int Initialize(CompletionOnceCallback callback) override;
  int Write(IOBuffer* buffer,
            int num_bytes,
            CompletionOnceCallback callback) override;
  int Finish(int net_error, CompletionOnceCallback callback) override;
  URLFetcherStringWriter* AsStringWriter() override;

 private:
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherStringWriter);
};

// URLFetcherResponseWriter implementation for files.
class NET_EXPORT URLFetcherFileWriter : public URLFetcherResponseWriter {
 public:
  // |file_path| is used as the destination path. If |file_path| is empty,
  // Initialize() will create a temporary file. The destination file is deleted
  // when a URLFetcherFileWriter instance is destructed unless DisownFile() is
  // called.
  URLFetcherFileWriter(
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      const base::FilePath& file_path);
  ~URLFetcherFileWriter() override;

  const base::FilePath& file_path() const { return file_path_; }

  // URLFetcherResponseWriter overrides:
  int Initialize(CompletionOnceCallback callback) override;
  int Write(IOBuffer* buffer,
            int num_bytes,
            CompletionOnceCallback callback) override;
  int Finish(int net_error, CompletionOnceCallback callback) override;
  URLFetcherFileWriter* AsFileWriter() override;

  // Drops ownership of the file at |file_path_|.
  // This class will not delete it or write to it again.
  void DisownFile();

 private:
  // Closes the file if it is open and then delete it.
  void CloseAndDeleteFile();

  // Callback which gets the result of a temporary file creation.
  void DidCreateTempFile(base::FilePath* temp_file_path, bool success);

  // Run |callback_| if it is non-null when FileStream::Open or
  // FileStream::Write is completed.
  void OnIOCompleted(int result);

  // Callback which gets the result of closing a file.
  void CloseComplete(int result);

  // Task runner on which file operations should happen.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Destination file path.
  // Initialize() creates a temporary file if this variable is empty.
  base::FilePath file_path_;

  // True when this instance is responsible to delete the file at |file_path_|.
  bool owns_file_;

  std::unique_ptr<FileStream> file_stream_;

  CompletionOnceCallback callback_;

  base::WeakPtrFactory<URLFetcherFileWriter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(URLFetcherFileWriter);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_FETCHER_RESPONSE_WRITER_H_
