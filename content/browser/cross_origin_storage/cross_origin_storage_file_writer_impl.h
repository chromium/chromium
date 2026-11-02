// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_FILE_WRITER_IMPL_H_
#define CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_FILE_WRITER_IMPL_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_registry.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_staging_writer.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_types.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_writer.mojom.h"
#include "url/origin.h"

namespace content {

// Browser-process implementation of `FileSystemAccessFileWriter` for
// Cross-Origin Storage.
//
// All bytes received from the renderer via `Write()` are streamed over a Mojo
// data pipe into a browser-owned quarantine file (`staging/<uuid>.tmp`) while
// `CrossOriginStorageStagingWriter` computes the SHA-256 digest in flight.
// Only when `Close()` is called does `CrossOriginStorageRegistry::Commit()`
// verify that the SHA-256 digest matches `hash_` and atomically move the
// staging file into `<Profile>/CrossOriginStorage/blobs/sha256/<hash>`.
class CONTENT_EXPORT CrossOriginStorageFileWriterImpl
    : public blink::mojom::FileSystemAccessFileWriter {
 public:
  CrossOriginStorageFileWriterImpl(
      base::WeakPtr<CrossOriginStorageRegistry> registry,
      CrossOriginStorageHash hash,
      url::Origin origin,
      CrossOriginStorageAccessSpec access_spec,
      base::SequenceBound<CrossOriginStorageStagingWriter> staging_writer,
      base::OnceClosure on_write_committed);

  CrossOriginStorageFileWriterImpl(const CrossOriginStorageFileWriterImpl&) =
      delete;
  CrossOriginStorageFileWriterImpl& operator=(
      const CrossOriginStorageFileWriterImpl&) = delete;
  ~CrossOriginStorageFileWriterImpl() override;

  // blink::mojom::FileSystemAccessFileWriter:
  void Write(uint64_t offset,
             mojo::ScopedDataPipeConsumerHandle stream,
             WriteCallback callback) override;
  void Truncate(uint64_t length, TruncateCallback callback) override;
  void Close(CloseCallback callback) override;
  void Abort(AbortCallback callback) override;

 private:
  bool has_pending_operation() const {
    return !active_write_callback_.is_null() ||
           !active_truncate_callback_.is_null();
  }

  void OnWriteStreamFinished(CrossOriginStorageStatus status,
                             uint64_t bytes_written);
  void OnTruncateFinished(CrossOriginStorageStatus status);
  void OnStagingFinished(CloseCallback callback,
                         CrossOriginStorageStagingWriter::Result result);
  static void OnCommitted(base::WeakPtr<CrossOriginStorageFileWriterImpl> self,
                          base::OnceClosure on_write_committed,
                          CloseCallback callback,
                          CrossOriginStorageStatus status);

  const base::WeakPtr<CrossOriginStorageRegistry> registry_;
  const CrossOriginStorageHash hash_;
  const url::Origin origin_;
  CrossOriginStorageAccessSpec access_spec_;
  base::OnceClosure on_write_committed_;

  base::SequenceBound<CrossOriginStorageStagingWriter> staging_writer_;

  // State for an active `Write()` or `Truncate()` operation.
  WriteCallback active_write_callback_;
  TruncateCallback active_truncate_callback_;
  const scoped_refptr<CrossOriginStorageStagingWriter::CancellationFlag>
      cancelled_;
  bool closed_or_aborted_ = false;

  base::WeakPtrFactory<CrossOriginStorageFileWriterImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_FILE_WRITER_IMPL_H_
