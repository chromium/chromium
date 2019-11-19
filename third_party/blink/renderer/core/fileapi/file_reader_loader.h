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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FILEAPI_FILE_READER_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FILEAPI_FILE_READER_LOADER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class BlobDataHandle;
class DOMArrayBuffer;
class FileReaderLoaderClient;
class TextResourceDecoder;

// Reads a Blob's content into memory.
//
// Blobs are typically stored on disk, and should be read asynchronously
// whenever possible. Synchronous loading is implemented to support Web Platform
// features that we cannot (yet) remove, such as FileReaderSync and synchronous
// XMLHttpRequest.
//
// Each FileReaderLoader instance is only good for reading one Blob, and will
// leak resources if used multiple times.
class CORE_EXPORT FileReaderLoader : public mojom::blink::BlobReaderClient {
  USING_FAST_MALLOC(FileReaderLoader);

 public:
  enum ReadType {
    kReadAsArrayBuffer,
    kReadAsBinaryString,
    kReadAsText,
    kReadAsDataURL,
    kReadByClient
  };

  // If client is given, do the loading asynchronously. Otherwise, load
  // synchronously.
  FileReaderLoader(ReadType,
                   FileReaderLoaderClient*,
                   scoped_refptr<base::SingleThreadTaskRunner>);
  ~FileReaderLoader() override;

  void Start(scoped_refptr<BlobDataHandle>);
  void Cancel();

  DOMArrayBuffer* ArrayBufferResult();
  String StringResult();
  ArrayBufferContents TakeContents();

  // Returns the total bytes received. Bytes ignored by m_rawData won't be
  // counted.
  //
  // This value doesn't grow more than numeric_limits<unsigned> when
  // m_readType is not set to ReadByClient.
  uint64_t BytesLoaded() const { return bytes_loaded_; }

  // Before OnCalculatedSize() is called: Returns nullopt.
  // After OnCalculatedSize() is called: Returns the size of the resource.
  base::Optional<uint64_t> TotalBytes() const { return total_bytes_; }

  FileErrorCode GetErrorCode() const { return error_code_; }

  int32_t GetNetError() const { return net_error_; }

  void SetEncoding(const String&);
  void SetDataType(const String& data_type) { data_type_ = data_type; }

  bool HasFinishedLoading() const { return finished_loading_; }

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FailureType {
    kMojoPipeCreation = 0,
    kSyncDataNotAllLoaded = 1,
    kSyncOnCompleteNotReceived = 2,
    kTotalBytesTooLarge = 3,
    kArrayBufferBuilderCreation = 4,
    kArrayBufferBuilderAppend = 5,
    kBackendReadError = 6,
    kReadSizesIncorrect = 7,
    kDataPipeNotReadableWithBytesLeft = 8,
    kMojoPipeClosedEarly = 9,
    // Any MojoResult error we aren't expecting during data pipe reading falls
    // into this bucket. If there are a large number of errors reported here,
    // then there can be a new enumeration reported for mojo pipe errors.
    kMojoPipeUnexpectedReadError = 10,
    kCount
  };

  void Cleanup();
  void Failed(FileErrorCode, FailureType type);

  void OnStartLoading(uint64_t total_bytes);
  void OnReceivedData(const char* data, unsigned data_length);
  void OnFinishLoading();

  bool IsSyncLoad() const { return !client_; }

  // BlobReaderClient:
  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override;
  void OnComplete(int32_t status, uint64_t data_length) override;
  void OnDataPipeReadable(MojoResult);

  String ConvertToText();
  String ConvertToDataURL();
  void SetStringResult(const String&);

  ReadType read_type_;
  FileReaderLoaderClient* client_;
  WTF::TextEncoding encoding_;
  String data_type_;

  ArrayBufferContents raw_data_;
  bool is_raw_data_converted_ = false;

  Persistent<DOMArrayBuffer> array_buffer_result_;
  String string_result_;

  // The decoder used to decode the text data.
  std::unique_ptr<TextResourceDecoder> decoder_;

  bool finished_loading_ = false;
  uint64_t bytes_loaded_ = 0;
  // total_bytes_ is set to the total size of the blob being loaded as soon as
  // it is known, and  the buffer for receiving data of total_bytes_ is
  // allocated and never grow even when extra data is appended.
  base::Optional<uint64_t> total_bytes_;

  int32_t net_error_ = 0;  // net::OK
  FileErrorCode error_code_ = FileErrorCode::kOK;

  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  mojo::SimpleWatcher handle_watcher_;
  mojo::Receiver<mojom::blink::BlobReaderClient> receiver_{this};
  bool received_all_data_ = false;
  bool received_on_complete_ = false;
#if DCHECK_IS_ON()
  bool started_loading_ = false;
#endif  // DCHECK_IS_ON()

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<FileReaderLoader> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FILEAPI_FILE_READER_LOADER_H_
