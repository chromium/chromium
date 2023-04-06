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

#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/system/wait.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader_client.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/blob/blob_url.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

DOMArrayBuffer* ToDOMArrayBuffer(ArrayBufferContents raw_data) {
  return DOMArrayBuffer::Create(std::move(raw_data));
}

String ToDataURL(ArrayBufferContents raw_data, const String& data_type = "") {
  StringBuilder builder;
  builder.Append("data:");

  if (!raw_data.IsValid() || !raw_data.DataLength()) {
    return builder.ToString();
  }

  if (data_type.empty()) {
    // Match Firefox in defaulting to application/octet-stream when the MIME
    // type is unknown. See https://crbug.com/48368.
    builder.Append("application/octet-stream");
  } else {
    builder.Append(data_type);
  }
  builder.Append(";base64,");

  Vector<char> out;
  Base64Encode(
      base::make_span(static_cast<const uint8_t*>(raw_data.Data()),
                      base::checked_cast<unsigned>(raw_data.DataLength())),
      out);
  builder.Append(out.data(), out.size());

  return builder.ToString();
}

String ToBinaryString(ArrayBufferContents raw_data) {
  CHECK(raw_data.IsValid());
  return String(static_cast<const char*>(raw_data.Data()),
                static_cast<size_t>(raw_data.DataLength()));
}

static inline String ToTextString(ArrayBufferContents raw_data,
                                  const WTF::TextEncoding& encoding) {
  if (!raw_data.IsValid() || !raw_data.DataLength()) {
    return "";
  }
  // Decode the data.
  // The File API spec says that we should use the supplied encoding if it is
  // valid. However, we choose to ignore this requirement in order to be
  // consistent with how WebKit decodes the web content: always has the BOM
  // override the provided encoding.
  // FIXME: consider supporting incremental decoding to improve the perf.
  StringBuilder builder;
  auto decoder = TextResourceDecoder(TextResourceDecoderOptions(
      TextResourceDecoderOptions::kPlainTextContent,
      encoding.IsValid() ? encoding : UTF8Encoding()));
  builder.Append(decoder.Decode(static_cast<const char*>(raw_data.Data()),
                                static_cast<size_t>(raw_data.DataLength())));

  builder.Append(decoder.Flush());

  return builder.ToString();
}

String ToString(ArrayBufferContents raw_data,
                FileReadType read_type,
                const WTF::TextEncoding& encoding,
                const String& data_type) {
  switch (read_type) {
    case FileReadType::kReadAsBinaryString:
      return ToBinaryString(std::move(raw_data));
    case FileReadType::kReadAsText:
      return ToTextString(std::move(raw_data), std::move(encoding));
    case FileReadType::kReadAsDataURL:
      return ToDataURL(std::move(raw_data), std::move(data_type));
    default:
      NOTREACHED();
  }
  return "";
}

}  // namespace

FileReaderLoader::FileReaderLoader(
    FileReadType read_type,
    FileReaderLoaderClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : read_type_(read_type),
      client_(client),
      handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
                      task_runner),
      task_runner_(std::move(task_runner)) {
  DCHECK(task_runner_);
}

FileReaderLoader::~FileReaderLoader() = default;

void FileReaderLoader::Start(scoped_refptr<BlobDataHandle> blob_data) {
#if DCHECK_IS_ON()
  DCHECK(!started_loading_) << "FileReaderLoader can only be used once";
  started_loading_ = true;
#endif  // DCHECK_IS_ON()

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      blink::BlobUtils::GetDataPipeCapacity(blob_data->size());

  mojo::ScopedDataPipeProducerHandle producer_handle;
  MojoResult rv = CreateDataPipe(&options, producer_handle, consumer_handle_);
  if (rv != MOJO_RESULT_OK) {
    Failed(FileErrorCode::kNotReadableErr);
    return;
  }

  blob_data->ReadAll(std::move(producer_handle),
                     receiver_.BindNewPipeAndPassRemote());

  if (IsSyncLoad()) {
    // Wait for OnCalculatedSize, which will also synchronously drain the data
    // pipe.
    receiver_.WaitForIncomingCall();
    if (received_on_complete_)
      return;
    if (!received_all_data_) {
      Failed(FileErrorCode::kNotReadableErr);
      return;
    }

    // Wait for OnComplete
    receiver_.WaitForIncomingCall();
    if (!received_on_complete_) {
      Failed(FileErrorCode::kNotReadableErr);
    }
  }
}

void FileReaderLoader::Cancel() {
  error_code_ = FileErrorCode::kAbortErr;
  Cleanup();
}

DOMArrayBuffer* FileReaderLoader::ArrayBufferResult() {
  DCHECK_EQ(read_type_, FileReadType::kReadAsArrayBuffer);

  // In practice, no clients are making those calls for partial data.
  // In case partial data is interesting, clients will use kReadByClient.
  CHECK(finished_loading_);

  return ToDOMArrayBuffer(std::move(raw_data_));
}

String FileReaderLoader::StringResult() {
  DCHECK_NE(read_type_, FileReadType::kReadAsArrayBuffer);
  DCHECK_NE(read_type_, FileReadType::kReadByClient);

  // In practice, no clients are making those calls for partial data.
  // In case partial data is interesting, clients will use kReadByClient.
  CHECK(finished_loading_);

  return ToString(std::move(raw_data_), read_type_, encoding_, data_type_);
}

ArrayBufferContents FileReaderLoader::TakeContents() {
  if (!raw_data_.IsValid() || error_code_ != FileErrorCode::kOK)
    return ArrayBufferContents();

  CHECK(finished_loading_);
  ArrayBufferContents contents = std::move(raw_data_);
  return contents;
}

void FileReaderLoader::SetEncoding(const String& encoding) {
  if (!encoding.empty())
    encoding_ = WTF::TextEncoding(encoding);
}

void FileReaderLoader::Cleanup() {
  handle_watcher_.Cancel();
  consumer_handle_.reset();
  receiver_.reset();

  // If we get any error, we do not need to keep a buffer around.
  if (error_code_ != FileErrorCode::kOK) {
    raw_data_.Reset();
  }
}

void FileReaderLoader::Failed(FileErrorCode error_code) {
  // If an error was already reported, don't report this error again.
  if (error_code_ != FileErrorCode::kOK)
    return;
  error_code_ = error_code;
  Cleanup();
  if (client_)
    client_->DidFail(error_code_);
}

void FileReaderLoader::OnStartLoading(uint64_t total_bytes) {
  total_bytes_ = total_bytes;

  DCHECK(!raw_data_.IsValid());

  if (read_type_ != FileReadType::kReadByClient) {
    // Check that we can cast to unsigned since we have to do
    // so to call ArrayBuffer's create function.
    // FIXME: Support reading more than the current size limit of ArrayBuffer.
    if (total_bytes > std::numeric_limits<unsigned>::max()) {
      Failed(FileErrorCode::kNotReadableErr);
      return;
    }

    raw_data_ = ArrayBufferContents(static_cast<unsigned>(total_bytes), 1,
                                    ArrayBufferContents::kNotShared,
                                    ArrayBufferContents::kDontInitialize);
    if (!raw_data_.IsValid()) {
      Failed(FileErrorCode::kNotReadableErr);
      return;
    }
  }

  if (client_)
    client_->DidStartLoading();
}

void FileReaderLoader::OnReceivedData(const char* data, unsigned data_length) {
  DCHECK(data);

  // Bail out if we already encountered an error.
  if (error_code_ != FileErrorCode::kOK)
    return;

  if (read_type_ == FileReadType::kReadByClient) {
    bytes_loaded_ += data_length;

    if (client_)
      client_->DidReceiveDataForClient(data, data_length);
    return;
  }

  // Receiving more data than expected would indicate a bug in the
  // implementation of the mojom Blob interface. However there is no guarantee
  // that the Blob is actually backed by a "real" blob, so to
  // defend against compromised renderer processes we still need to carefully
  // validate anything received. So return an error if we received too much
  // data.
  if (bytes_loaded_ + data_length > raw_data_.DataLength()) {
    raw_data_.Reset();
    bytes_loaded_ = 0;
    Failed(FileErrorCode::kNotReadableErr);
    return;
  }
  memcpy(static_cast<char*>(raw_data_.Data()) + bytes_loaded_, data,
         data_length);
  bytes_loaded_ += data_length;

  if (client_)
    client_->DidReceiveData();
}

void FileReaderLoader::OnFinishLoading() {
  if (read_type_ != FileReadType::kReadByClient && raw_data_.IsValid()) {
    DCHECK_EQ(bytes_loaded_, raw_data_.DataLength());
  }

  finished_loading_ = true;

  Cleanup();
  if (client_)
    client_->DidFinishLoading();
}

void FileReaderLoader::OnCalculatedSize(uint64_t total_size,
                                        uint64_t expected_content_size) {
  auto weak_this = WrapWeakPersistent(this);
  OnStartLoading(expected_content_size);
  // OnStartLoading calls out to our client, which could delete |this|, so bail
  // out if that happened.
  if (!weak_this)
    return;

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
    Failed(status == net::ERR_FILE_NOT_FOUND ? FileErrorCode::kNotFoundErr
                                             : FileErrorCode::kNotReadableErr);
    return;
  }
  if (data_length != total_bytes_) {
    Failed(FileErrorCode::kNotReadableErr);
    return;
  }

  received_on_complete_ = true;
  if (received_all_data_)
    OnFinishLoading();
}

void FileReaderLoader::OnDataPipeReadable(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    if (!received_all_data_) {
      Failed(FileErrorCode::kNotReadableErr);
    }
    return;
  }

  while (true) {
    uint32_t num_bytes;
    const void* buffer;
    MojoResult pipe_result = consumer_handle_->BeginReadData(
        &buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
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
        Failed(FileErrorCode::kNotReadableErr);
      }
      return;
    }
    if (pipe_result != MOJO_RESULT_OK) {
      Failed(FileErrorCode::kNotReadableErr);
      return;
    }

    auto weak_this = WrapWeakPersistent(this);
    OnReceivedData(static_cast<const char*>(buffer), num_bytes);
    // OnReceivedData calls out to our client, which could delete |this|, so
    // bail out if that happened.
    if (!weak_this)
      return;

    consumer_handle_->EndReadData(num_bytes);
    if (BytesLoaded() >= total_bytes_) {
      received_all_data_ = true;
      if (received_on_complete_)
        OnFinishLoading();
      return;
    }
  }
}

}  // namespace blink
