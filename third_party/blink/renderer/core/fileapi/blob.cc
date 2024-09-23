/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/fileapi/blob.h"

#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob_property_bag.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_blob_usvstring.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fileapi/file_read_type.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/url/dom_url.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/blob/blob_url.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// http://dev.w3.org/2006/webapi/FileAPI/#constructorBlob
bool IsValidBlobType(const String& type) {
  for (unsigned i = 0; i < type.length(); ++i) {
    UChar c = type[i];
    if (c < 0x20 || c > 0x7E) {
      return false;
    }
  }
  return true;
}

}  // namespace

// TODO(https://crbug.com/989876): This is not used any more, refactor
// PublicURLManager to deprecate this.
class NullURLRegistry final : public URLRegistry {
 public:
  void RegisterURL(const KURL&, URLRegistrable*) override {}
  void UnregisterURL(const KURL&) override {}
};

// Helper class to asynchronously read from a Blob using a FileReaderLoader.
// Each client is only good for one Blob read operation.
// This class is not thread-safe.
class BlobFileReaderClient : public GarbageCollected<BlobFileReaderClient>,
                             public FileReaderAccumulator {
 public:
  BlobFileReaderClient(
      const scoped_refptr<BlobDataHandle> blob_data_handle,
      const scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const FileReadType read_type,
      ScriptPromiseResolverBase* resolver)
      : loader_(MakeGarbageCollected<FileReaderLoader>(this,
                                                       std::move(task_runner))),
        resolver_(resolver),
        read_type_(read_type),
        keep_alive_(this) {
    loader_->Start(std::move(blob_data_handle));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(loader_);
    visitor->Trace(resolver_);
    FileReaderAccumulator::Trace(visitor);
  }

  ~BlobFileReaderClient() override = default;
  void DidFail(FileErrorCode error_code) override {
    FileReaderAccumulator::DidFail(error_code);
    resolver_->Reject(file_error::CreateDOMException(error_code));
    Done();
  }

  void DidFinishLoading(FileReaderData contents) override {
    if (read_type_ == FileReadType::kReadAsText) {
      String result = std::move(contents).AsText("UTF-8");
      resolver_->DowncastTo<IDLUSVString>()->Resolve(result);
    } else if (read_type_ == FileReadType::kReadAsArrayBuffer) {
      DOMArrayBuffer* result = std::move(contents).AsDOMArrayBuffer();
      resolver_->DowncastTo<DOMArrayBuffer>()->Resolve(result);
    } else {
      NOTREACHED_IN_MIGRATION()
          << "Unknown ReadType supplied to BlobFileReaderClient";
    }
    Done();
  }

 private:
  void Done() {
    keep_alive_.Clear();
    loader_ = nullptr;
  }
  Member<FileReaderLoader> loader_;
  Member<ScriptPromiseResolverBase> resolver_;
  const FileReadType read_type_;
  SelfKeepAlive<BlobFileReaderClient> keep_alive_;
};

Blob::Blob(scoped_refptr<BlobDataHandle> data_handle)
    : blob_data_handle_(std::move(data_handle)) {}

Blob::~Blob() = default;

// static
Blob* Blob::Create(ExecutionContext* context,
                   const HeapVector<Member<V8BlobPart>>& blob_parts,
                   const BlobPropertyBag* options) {
  DCHECK(options->hasType());
  DCHECK(options->hasEndings());
  bool normalize_line_endings_to_native = (options->endings() == "native");
  if (normalize_line_endings_to_native)
    UseCounter::Count(context, WebFeature::kFileAPINativeLineEndings);
  UseCounter::Count(context, WebFeature::kCreateObjectBlob);

  auto blob_data = std::make_unique<BlobData>();
  blob_data->SetContentType(NormalizeType(options->type()));

  PopulateBlobData(blob_data.get(), blob_parts,
                   normalize_line_endings_to_native);

  uint64_t blob_size = blob_data->length();
  return MakeGarbageCollected<Blob>(
      BlobDataHandle::Create(std::move(blob_data), blob_size));
}

Blob* Blob::Create(base::span<const uint8_t> data, const String& content_type) {
  auto blob_data = std::make_unique<BlobData>();
  blob_data->SetContentType(content_type);
  blob_data->AppendBytes(data);
  uint64_t blob_size = blob_data->length();

  return MakeGarbageCollected<Blob>(
      BlobDataHandle::Create(std::move(blob_data), blob_size));
}

// static
void Blob::PopulateBlobData(BlobData* blob_data,
                            const HeapVector<Member<V8BlobPart>>& parts,
                            bool normalize_line_endings_to_native) {
  for (const auto& item : parts) {
    switch (item->GetContentType()) {
      case V8BlobPart::ContentType::kArrayBuffer: {
        DOMArrayBuffer* array_buffer = item->GetAsArrayBuffer();
        blob_data->AppendBytes(array_buffer->ByteSpan());
        break;
      }
      case V8BlobPart::ContentType::kArrayBufferView: {
        auto&& array_buffer_view = item->GetAsArrayBufferView();
        blob_data->AppendBytes(array_buffer_view->ByteSpan());
        break;
      }
      case V8BlobPart::ContentType::kBlob: {
        item->GetAsBlob()->AppendTo(*blob_data);
        break;
      }
      case V8BlobPart::ContentType::kUSVString: {
        blob_data->AppendText(item->GetAsUSVString(),
                              normalize_line_endings_to_native);
        break;
      }
    }
  }
}

// static
void Blob::ClampSliceOffsets(uint64_t size, int64_t& start, int64_t& end) {
  DCHECK_NE(size, std::numeric_limits<uint64_t>::max());

  // Convert the negative value that is used to select from the end.
  if (start < 0)
    start = start + size;
  if (end < 0)
    end = end + size;

  // Clamp the range if it exceeds the size limit.
  if (start < 0)
    start = 0;
  if (end < 0)
    end = 0;
  if (static_cast<uint64_t>(start) >= size) {
    start = 0;
    end = 0;
  } else if (end < start) {
    end = start;
  } else if (static_cast<uint64_t>(end) > size) {
    end = size;
  }
}

Blob* Blob::slice(int64_t start,
                  int64_t end,
                  const String& content_type,
                  ExceptionState& exception_state) const {
  uint64_t size = this->size();
  ClampSliceOffsets(size, start, end);

  uint64_t length = end - start;
  auto blob_data = std::make_unique<BlobData>();
  blob_data->SetContentType(NormalizeType(content_type));
  blob_data->AppendBlob(blob_data_handle_, start, length);
  return MakeGarbageCollected<Blob>(
      BlobDataHandle::Create(std::move(blob_data), length));
}

ReadableStream* Blob::stream(ScriptState* script_state) const {
  BodyStreamBuffer* body_buffer = BodyStreamBuffer::Create(
      script_state,
      MakeGarbageCollected<BlobBytesConsumer>(
          ExecutionContext::From(script_state), blob_data_handle_),
      /*signal=*/nullptr, /*cached_metadata_handler=*/nullptr);

  return body_buffer->Stream();
}

ScriptPromise<IDLUSVString> Blob::text(ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUSVString>>(script_state);
  auto promise = resolver->Promise();
  MakeGarbageCollected<BlobFileReaderClient>(
      blob_data_handle_,
      ExecutionContext::From(script_state)
          ->GetTaskRunner(TaskType::kFileReading),
      FileReadType::kReadAsText, resolver);
  return promise;
}

ScriptPromise<DOMArrayBuffer> Blob::arrayBuffer(ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<DOMArrayBuffer>>(script_state);
  auto promise = resolver->Promise();
  MakeGarbageCollected<BlobFileReaderClient>(
      blob_data_handle_,
      ExecutionContext::From(script_state)
          ->GetTaskRunner(TaskType::kFileReading),
      FileReadType::kReadAsArrayBuffer, resolver);
  return promise;
}

scoped_refptr<BlobDataHandle> Blob::GetBlobDataHandleWithKnownSize() const {
  if (!blob_data_handle_->IsSingleUnknownSizeFile()) {
    return blob_data_handle_;
  }
  return BlobDataHandle::Create(blob_data_handle_->Uuid(),
                                blob_data_handle_->GetType(), size(),
                                blob_data_handle_->CloneBlobRemote());
}

void Blob::AppendTo(BlobData& blob_data) const {
  blob_data.AppendBlob(blob_data_handle_, 0, size());
}

URLRegistry& Blob::Registry() const {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(NullURLRegistry, instance, ());
  return instance;
}

bool Blob::IsMojoBlob() {
  return true;
}

void Blob::CloneMojoBlob(mojo::PendingReceiver<mojom::blink::Blob> receiver) {
  blob_data_handle_->CloneBlobRemote(std::move(receiver));
}

mojo::PendingRemote<mojom::blink::Blob> Blob::AsMojoBlob() const {
  return blob_data_handle_->CloneBlobRemote();
}

// static
String Blob::NormalizeType(const String& type) {
  if (type.IsNull()) {
    return g_empty_string;
  }
  if (type.length() > 65535) {
    return g_empty_string;
  }
  if (!IsValidBlobType(type)) {
    return g_empty_string;
  }
  return type.DeprecatedLower();
}

}  // namespace blink
