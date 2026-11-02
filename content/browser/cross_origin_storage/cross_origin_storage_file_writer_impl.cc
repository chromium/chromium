// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cross_origin_storage/cross_origin_storage_file_writer_impl.h"

#include <atomic>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"

namespace content {

CrossOriginStorageFileWriterImpl::CrossOriginStorageFileWriterImpl(
    base::WeakPtr<CrossOriginStorageRegistry> registry,
    CrossOriginStorageHash hash,
    url::Origin origin,
    CrossOriginStorageAccessSpec access_spec,
    base::SequenceBound<CrossOriginStorageStagingWriter> staging_writer,
    base::OnceClosure on_write_committed)
    : registry_(std::move(registry)),
      hash_(std::move(hash)),
      origin_(std::move(origin)),
      access_spec_(std::move(access_spec)),
      on_write_committed_(std::move(on_write_committed)),
      staging_writer_(std::move(staging_writer)),
      cancelled_(base::MakeRefCounted<
                 CrossOriginStorageStagingWriter::CancellationFlag>()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CrossOriginStorageFileWriterImpl::~CrossOriginStorageFileWriterImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cancelled_->data.store(true, std::memory_order_relaxed);
}

void CrossOriginStorageFileWriterImpl::Write(
    uint64_t offset,
    mojo::ScopedDataPipeConsumerHandle stream,
    WriteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (closed_or_aborted_ || has_pending_operation()) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kInvalidState,
            "Writer is closed or busy."),
        0);
    return;
  }

  active_write_callback_ = std::move(callback);
  staging_writer_.AsyncCall(&CrossOriginStorageStagingWriter::WriteStream)
      .WithArgs(offset, std::move(stream), cancelled_,
                base::BindPostTaskToCurrentDefault(base::BindOnce(
                    &CrossOriginStorageFileWriterImpl::OnWriteStreamFinished,
                    weak_factory_.GetWeakPtr())));
}

void CrossOriginStorageFileWriterImpl::OnWriteStreamFinished(
    CrossOriginStorageStatus status,
    uint64_t bytes_written) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (closed_or_aborted_ || !active_write_callback_) {
    return;
  }

  if (status != CrossOriginStorageStatus::kSuccess) {
    closed_or_aborted_ = true;
    blink::mojom::FileSystemAccessStatus fsa_status =
        status == CrossOriginStorageStatus::kQuotaExceeded
            ? blink::mojom::FileSystemAccessStatus::kQuotaExceededError
            : blink::mojom::FileSystemAccessStatus::kOperationFailed;
    std::move(active_write_callback_)
        .Run(file_system_access_error::FromStatus(fsa_status), 0);
    return;
  }

  std::move(active_write_callback_)
      .Run(file_system_access_error::Ok(), bytes_written);
}

void CrossOriginStorageFileWriterImpl::Truncate(uint64_t length,
                                                TruncateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (closed_or_aborted_ || has_pending_operation()) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kInvalidState,
        "Writer is closed or busy."));
    return;
  }

  active_truncate_callback_ = std::move(callback);
  staging_writer_.AsyncCall(&CrossOriginStorageStagingWriter::Truncate)
      .WithArgs(length)
      .Then(
          base::BindOnce(&CrossOriginStorageFileWriterImpl::OnTruncateFinished,
                         weak_factory_.GetWeakPtr()));
}

void CrossOriginStorageFileWriterImpl::OnTruncateFinished(
    CrossOriginStorageStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (closed_or_aborted_ || !active_truncate_callback_) {
    return;
  }

  if (status != CrossOriginStorageStatus::kSuccess) {
    closed_or_aborted_ = true;
    blink::mojom::FileSystemAccessStatus fsa_status =
        status == CrossOriginStorageStatus::kQuotaExceeded
            ? blink::mojom::FileSystemAccessStatus::kQuotaExceededError
            : blink::mojom::FileSystemAccessStatus::kOperationFailed;
    std::move(active_truncate_callback_)
        .Run(file_system_access_error::FromStatus(fsa_status));
    return;
  }

  std::move(active_truncate_callback_).Run(file_system_access_error::Ok());
}

void CrossOriginStorageFileWriterImpl::Close(CloseCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (closed_or_aborted_ || has_pending_operation()) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kInvalidState,
        "Writer is already closed or has a pending operation."));
    return;
  }

  closed_or_aborted_ = true;
  staging_writer_.AsyncCall(&CrossOriginStorageStagingWriter::Finish)
      .WithArgs(cancelled_)
      .Then(base::BindOnce(&CrossOriginStorageFileWriterImpl::OnStagingFinished,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CrossOriginStorageFileWriterImpl::OnStagingFinished(
    CloseCallback callback,
    CrossOriginStorageStagingWriter::Result result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (result.status != CrossOriginStorageStatus::kSuccess || !registry_) {
    blink::mojom::FileSystemAccessStatus fsa_status =
        result.status == CrossOriginStorageStatus::kQuotaExceeded
            ? blink::mojom::FileSystemAccessStatus::kQuotaExceededError
            : blink::mojom::FileSystemAccessStatus::kOperationFailed;
    std::move(callback).Run(file_system_access_error::FromStatus(
        fsa_status, "Failed to finalize staging file."));
    return;
  }

  registry_->Commit(
      hash_, origin_, std::move(access_spec_), std::move(result),
      base::BindOnce(&CrossOriginStorageFileWriterImpl::OnCommitted,
                     weak_factory_.GetWeakPtr(), std::move(on_write_committed_),
                     std::move(callback)));
}

// static
void CrossOriginStorageFileWriterImpl::OnCommitted(
    base::WeakPtr<CrossOriginStorageFileWriterImpl> self,
    base::OnceClosure on_write_committed,
    CloseCallback callback,
    CrossOriginStorageStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (status == CrossOriginStorageStatus::kSuccess && on_write_committed) {
    std::move(on_write_committed).Run();
  }
  if (!self) {
    return;
  }

  switch (status) {
    case CrossOriginStorageStatus::kSuccess:
      std::move(callback).Run(file_system_access_error::Ok());
      return;
    case CrossOriginStorageStatus::kDataError:
      std::move(callback).Run(file_system_access_error::FromStatus(
          blink::mojom::FileSystemAccessStatus::kDataError,
          "The written bytes do not match the declared cryptographic hash."));
      return;
    case CrossOriginStorageStatus::kQuotaExceeded:
      std::move(callback).Run(file_system_access_error::FromStatus(
          blink::mojom::FileSystemAccessStatus::kQuotaExceededError,
          "Cross-Origin Storage quota exceeded."));
      return;
    case CrossOriginStorageStatus::kNotFound:
    case CrossOriginStorageStatus::kInvalidArgument:
    case CrossOriginStorageStatus::kOperationFailed:
    case CrossOriginStorageStatus::kOperationAborted:
      std::move(callback).Run(file_system_access_error::FromStatus(
          blink::mojom::FileSystemAccessStatus::kOperationFailed,
          "Failed to commit Cross-Origin Storage resource."));
      return;
  }
}

void CrossOriginStorageFileWriterImpl::Abort(AbortCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (closed_or_aborted_) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kInvalidState,
        "An attempt was made to abort an already closed or aborted writer."));
    return;
  }

  closed_or_aborted_ = true;
  cancelled_->data.store(true, std::memory_order_relaxed);
  if (active_write_callback_) {
    std::move(active_write_callback_)
        .Run(file_system_access_error::FromStatus(
                 blink::mojom::FileSystemAccessStatus::kOperationAborted),
             0);
  }
  if (active_truncate_callback_) {
    std::move(active_truncate_callback_)
        .Run(file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationAborted));
  }

  staging_writer_.AsyncCall(&CrossOriginStorageStagingWriter::Discard)
      .Then(base::BindOnce(
          [](AbortCallback cb) {
            std::move(cb).Run(file_system_access_error::Ok());
          },
          std::move(callback)));
}

}  // namespace content
