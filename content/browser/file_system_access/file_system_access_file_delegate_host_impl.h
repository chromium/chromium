// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_DELEGATE_HOST_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_DELEGATE_HOST_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/thread_annotations.h"
#include "components/services/storage/public/cpp/big_io_buffer.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_delegate_host.mojom.h"

namespace content {

// Browser side implementation of the FileSystemAccessFileDelegateHost mojom
// interface, which facilitates file operations for Access Handles in incognito
// mode. Instances of this class are owned by the
// FileSystemAccessAccessHandleHostImpl instance of the associated URL, which
// constructs it.
class FileSystemAccessFileDelegateHostImpl
    : public blink::mojom::FileSystemAccessFileDelegateHost {
 public:
  FileSystemAccessFileDelegateHostImpl(
      FileSystemAccessManagerImpl* manager,
      const storage::FileSystemURL& url,
      base::PassKey<FileSystemAccessAccessHandleHostImpl> pass_key,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileDelegateHost>
          receiver);
  ~FileSystemAccessFileDelegateHostImpl() override;

  // blink::mojom::FileSystemAccessFileDelegateHost:
  void Read(int64_t offset, int bytes_to_read, ReadCallback callback) override;
  void Write(int64_t offset,
             mojo::ScopedDataPipeConsumerHandle data,
             WriteCallback callback) override;
  void GetLength(GetLengthCallback callback) override;
  void SetLength(int64_t length, SetLengthCallback callback) override;

 private:
  // State that is kept for the duration of a write operation, to keep track of
  // progress until the write completes.
  struct WriteState;

  void OnDisconnect();

  FileSystemAccessManagerImpl* manager() { return manager_; }
  storage::FileSystemContext* file_system_context() {
    return manager()->context();
  }
  const storage::FileSystemURL& url() { return url_; }

  void DidRead(scoped_refptr<storage::BigIOBuffer> buffer,
               ReadCallback callback,
               int rv);
  void DidWrite(WriteState* state,
                base::File::Error result,
                int64_t bytes,
                bool complete);

  // This is safe, since the manager owns the
  // FileSystemAccessAccessHandleHostImpl which owns this class.
  const raw_ptr<FileSystemAccessManagerImpl> manager_ = nullptr;
  const storage::FileSystemURL url_;

  mojo::Receiver<blink::mojom::FileSystemAccessFileDelegateHost> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FileSystemAccessFileDelegateHostImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_DELEGATE_HOST_IMPL_H_
