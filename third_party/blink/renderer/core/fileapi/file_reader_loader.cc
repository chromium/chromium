/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/system/wait.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"
#include "third_party/blink/renderer/platform/blob/blob_url.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

FileReaderLoader::FileReaderLoader(
    FileReaderClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : client_(client),
      handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
                      task_runner),
      task_runner_(std::move(task_runner)) {
  CHECK(client);
  DCHECK(task_runner_);
}

FileReaderLoader::~FileReaderLoader() = default;

void FileReaderLoader::Start(scoped_refptr<BlobDataHandle> blob_data) {
  StartInternal(std::move(blob_data), /*is_sync=*/false);
}

void FileReaderLoader::StartSync(scoped_refptr<BlobDataHandle> blob_data) {
  StartInternal(std::move(blob_data), /*is_sync=*/true);
}

void FileReaderLoader::StartInternal(scoped_refptr<BlobDataHandle> blob_data,
                                     bool is_sync) {
#if DCHECK_IS_ON()
  DCHECK(!started_loading_) << "FileReaderLoader can only be used once";
  started_loading_ = true;
#endif  // DCHECK_IS_ON()

  // This sets up the `IsSyncLoad` mechanism for the lifetime of this method.
  base::AutoReset<bool> scoped_is_sync(&is_sync_, is_sync);

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      blink::BlobUtils::GetDataPipeCapacity(blob_data->size());

  mojo::ScopedDataPipeProducerHandle producer_handle;
  MojoResult rv = CreateDataPipe(&options, producer_handle, consumer_handle_);
  if (rv != MOJO_RESULT_OK) {
    Failed(FileErrorCode::kNotReadableErr, FailureType::kMojoPipeCreation);
    return;
  }

  blob_data->ReadAll(std::move(producer_handle),
                     receiver_.BindNewPipeAndPassRemote(task_runner_));

  if (IsSyncLoad()) {
    // Wait for OnCalculatedSize, which will also synchronously drain the data
    // pipe.
    receiver_.WaitForIncomingCall();
    if (received_on_complete_)
      return;
    if (!received_all_data_) {
      Failed(FileErrorCode::kNotReadableErr,
             FailureType::kSyncDataNotAllLoaded);
      return;
    }

    // Wait for OnComplete
    receiver_.WaitForIncomingCall();
    if (!received_on_complete_) {
      Failed(FileErrorCode::kNotReadableErr,
             FailureType::kSyncOnCompleteNotReceived);
    }
  }
}

void FileReaderLoader::Cancel() {
  error_code_ = FileErrorCode::kAbortErr;
  Cleanup();
}

void FileReaderLoader::Cleanup() {
  handle_watcher_.Cancel();
  consumer_handle_.reset();
  receiver_.reset();
}

void FileReaderLoader::Failed(FileErrorCode error_code, FailureType type) {
  // If an error was already reported, don't report this error again.
  if (error_code_ != FileErrorCode::kOK)
    return;
  error_code_ = error_code;
  base::UmaHistogramEnumeration("Storage.Blob.FileReaderLoader.FailureType2",
                                type);
  Cleanup();
  client_->DidFail(error_code_);
}

void FileReaderLoader::OnFinishLoading() {
  finished_loading_ = true;
  Cleanup();
  client_->DidFinishLoading();
}

void FileReaderLoader::OnCalculatedSize(uint64_t total_size,
                                        uint64_t expected_content_size) {
  total_bytes_ = expected_content_size;

  if (auto err = client_->DidStartLoading(expected_content_size);
      err != FileErrorCode::kOK) {
    Failed(err, FailureType::kClientFailure);
    return;
  }

  if (expected_content_size == 0) {
    received_all_data_ = true;
    return;
  }

  if (IsSyncLoad()) {
    OnDataPipeReadable(MOJO_RESULT_OK);
  } else {
    handle_watcher_.Watch(
        consumer_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
        WTF::BindRepeating(&FileReaderLoader::OnDataPipeReadable,
                           WrapWeakPersistent(this)));
  }
}

void FileReaderLoader::OnComplete(int32_t status, uint64_t data_length) {
  if (status != net::OK) {
    net_error_ = status;
    base::UmaHistogramSparse("Storage.Blob.FileReaderLoader.ReadError2",
                             std::max(0, -net_error_));
    Failed(status == net::ERR_FILE_NOT_FOUND ? FileErrorCode::kNotFoundErr
                                             : FileErrorCode::kNotReadableErr,
           FailureType::kBackendReadError);
    return;
  }
  if (data_length != total_bytes_) {
    Failed(FileErrorCode::kNotReadableErr, FailureType::kReadSizesIncorrect);
    return;
  }

  received_on_complete_ = true;
  if (received_all_data_)
    OnFinishLoading();
}

void FileReaderLoader::OnDataPipeReadable(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    if (!received_all_data_ && result != MOJO_RESULT_FAILED_PRECONDITION) {
      // Whatever caused a `MOJO_RESULT_FAILED_PRECONDITION` will also prevent
      // `BlobDataHandle` from writing to the pipe, so we expect a call to
      // `OnComplete()` soon with a more specific error that we will then pass
      // to the client.
      base::UmaHistogramExactLinear(
          "Storage.Blob.FileReaderLoader.DataPipeNotReadableMojoError", result,
          MOJO_RESULT_SHOULD_WAIT + 1);
      Failed(FileErrorCode::kNotReadableErr,
             FailureType::kDataPipeNotReadableWithBytesLeft);
    }
    return;
  }

  while (true) {
    base::span<const uint8_t> buffer;
    MojoResult pipe_result =
        consumer_handle_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
    if (pipe_result == MOJO_RESULT_SHOULD_WAIT) {
      if (!IsSyncLoad())
        return;

      pipe_result =
          mojo::Wait(consumer_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE);
      if (pipe_result == MOJO_RESULT_OK)
        continue;
    }
    if (pipe_result == MOJO_RESULT_FAILED_PRECONDITION) {
      // Pipe closed.
      if (!received_all_data_) {
        Failed(FileErrorCode::kNotReadableErr,
               FailureType::kMojoPipeClosedEarly);
      }
      return;
    }
    if (pipe_result != MOJO_RESULT_OK) {
      Failed(FileErrorCode::kNotReadableErr,
             FailureType::kMojoPipeUnexpectedReadError);
      return;
    }

    DCHECK(buffer.data());
    DCHECK_EQ(error_code_, FileErrorCode::kOK);

    bytes_loaded_ += buffer.size();

    if (auto err = client_->DidReceiveData(buffer); err != FileErrorCode::kOK) {
      Failed(err, FailureType::kClientFailure);
      return;
    }

    consumer_handle_->EndReadData(buffer.size());
    if (BytesLoaded() >= total_bytes_) {
      received_all_data_ = true;
      if (received_on_complete_)
        OnFinishLoading();
      return;
    }
  }
}

}  // namespace blink
