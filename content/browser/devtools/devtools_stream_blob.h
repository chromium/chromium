// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_STREAM_BLOB_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_STREAM_BLOB_H_

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/blob_storage_constants.h"

#include <memory>

namespace net {
class IOBufferWithSize;
}

namespace storage {
class BlobDataHandle;
class BlobReader;
}  // namespace storage

namespace content {
class ChromeBlobStorageContext;
class StoragePartition;

class DevToolsStreamBlob : public DevToolsIOContext::Stream {
 public:
  using OpenCallback = base::OnceCallback<void(bool)>;

  static scoped_refptr<DevToolsIOContext::Stream> Create(
      DevToolsIOContext* io_context,
      ChromeBlobStorageContext* blob_context,
      StoragePartition* partition,
      const std::string& handle,
      const std::string& uuid);

 private:
  DevToolsStreamBlob();

  void Open(scoped_refptr<ChromeBlobStorageContext> context,
            StoragePartition* partition,
            const std::string& handle,
            OpenCallback callback);

  void Read(off_t position, size_t max_size, ReadCallback callback) override;

  struct ReadRequest {
    off_t position;
    size_t max_size;
    ReadCallback callback;

    void Fail();

    ReadRequest() = delete;
    ReadRequest(off_t position, size_t max_size, ReadCallback callback);
    ~ReadRequest();
  };

  ~DevToolsStreamBlob() override;

  void OpenOnIO(scoped_refptr<ChromeBlobStorageContext> blob_context,
                const std::string& uuid,
                OpenCallback callback);
  void ReadOnIO(std::unique_ptr<ReadRequest> request);
  void CloseOnIO(bool invoke_pending_callbacks);

  void FailOnIO();
  void FailOnIO(OpenCallback callback);

  void StartReadRequest();
  void CreateReader();
  void BeginRead();

  void OnReadComplete(int bytes_read);
  void OnBlobConstructionComplete(storage::BlobStatus status);
  void OnCalculateSizeComplete(int net_error);

  std::unique_ptr<storage::BlobDataHandle> blob_handle_;
  OpenCallback open_callback_;
  std::unique_ptr<storage::BlobReader> blob_reader_;
  base::queue<std::unique_ptr<ReadRequest>> pending_reads_;
  scoped_refptr<net::IOBufferWithSize> io_buf_;
  off_t last_read_pos_;
  bool failed_;
  bool is_binary_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_STREAM_BLOB_H_
