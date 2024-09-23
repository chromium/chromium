// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/compiler_specific.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/renderer/core/fetch/multipart_parser.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/parsed_content_disposition.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class FetchDataLoaderAsBlobHandle final : public FetchDataLoader,
                                          public FetchDataLoader::Client {
 public:
  FetchDataLoaderAsBlobHandle(
      const String& mime_type,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : mime_type_(mime_type), task_runner_(std::move(task_runner)) {}

  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!client_);
    DCHECK(!consumer_);

    client_ = client;
    consumer_ = consumer;

    scoped_refptr<BlobDataHandle> blob_handle =
        consumer_->DrainAsBlobDataHandle();
    if (blob_handle) {
      DCHECK_NE(UINT64_MAX, blob_handle->size());
      if (blob_handle->GetType() != mime_type_) {
        // A new Blob is created to override the Blob's type.
        auto blob_size = blob_handle->size();
        auto blob_data = std::make_unique<BlobData>();
        blob_data->SetContentType(mime_type_);
        blob_data->AppendBlob(std::move(blob_handle), 0, blob_size);
        client_->DidFetchDataLoadedBlobHandle(
            BlobDataHandle::Create(std::move(blob_data), blob_size));
      } else {
        client_->DidFetchDataLoadedBlobHandle(std::move(blob_handle));
      }
      return;
    }

    data_pipe_loader_ = CreateLoaderAsDataPipe(task_runner_);
    data_pipe_loader_->Start(consumer_, this);
  }

  void Cancel() override {
    load_canceled_ = true;
    blob_handle_.reset();
    consumer_->Cancel();
  }

  void DidFetchDataStartedDataPipe(
      mojo::ScopedDataPipeConsumerHandle handle) override {
    DCHECK(BlobDataHandle::GetBlobRegistry());
    BlobDataHandle::GetBlobRegistry()->RegisterFromStream(
        mime_type_ ? mime_type_ : "", /*content_disposition=*/"",
        /*length_hint=*/0, std::move(handle),
        mojo::PendingAssociatedRemote<mojom::blink::ProgressClient>(),
        WTF::BindOnce(
            &FetchDataLoaderAsBlobHandle::FinishedCreatingFromDataPipe,
            WrapWeakPersistent(this)));
  }

  void DidFetchDataLoadedDataPipe() override {
    DCHECK(!load_complete_);
    load_complete_ = true;
    if (blob_handle_)
      client_->DidFetchDataLoadedBlobHandle(std::move(blob_handle_));
  }

  void DidFetchDataLoadFailed() override { client_->DidFetchDataLoadFailed(); }

  void Abort() override { client_->Abort(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    visitor->Trace(data_pipe_loader_);
    FetchDataLoader::Trace(visitor);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  void FinishedCreatingFromDataPipe(
      const scoped_refptr<BlobDataHandle>& blob_handle) {
    if (load_canceled_)
      return;
    if (!blob_handle) {
      DidFetchDataLoadFailed();
      return;
    }
    if (!load_complete_) {
      blob_handle_ = blob_handle;
      return;
    }
    client_->DidFetchDataLoadedBlobHandle(blob_handle);
  }

  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;
  Member<FetchDataLoader> data_pipe_loader_;

  const String mime_type_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<BlobDataHandle> blob_handle_;
  bool load_complete_ = false;
  bool load_canceled_ = false;
};

class FetchDataLoaderAsArrayBuffer final : public FetchDataLoader,
                                           public BytesConsumer::Client {
 public:
  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!client_);
    DCHECK(!consumer_);
    DCHECK(!buffer_);
    client_ = client;
    consumer_ = consumer;
    buffer_ = WTF::SharedBuffer::Create();
    consumer_->SetClient(this);
    OnStateChange();
  }

  void Cancel() override { consumer_->Cancel(); }

  void OnStateChange() override {
    while (true) {
      const char* buffer;
      size_t available;
      auto result = consumer_->BeginRead(&buffer, &available);
      if (result == BytesConsumer::Result::kShouldWait)
        return;
      if (result == BytesConsumer::Result::kOk) {
        if (available > 0) {
          bool ok = Append(buffer, base::checked_cast<wtf_size_t>(available));
          if (!ok) {
            [[maybe_unused]] auto unused = consumer_->EndRead(0);
            consumer_->Cancel();
            client_->DidFetchDataLoadFailed();
            return;
          }
        }
        result = consumer_->EndRead(available);
      }
      switch (result) {
        case BytesConsumer::Result::kOk:
          break;
        case BytesConsumer::Result::kShouldWait:
          NOTREACHED_IN_MIGRATION();
          return;
        case BytesConsumer::Result::kDone: {
          DOMArrayBuffer* array_buffer = BuildArrayBuffer();
          if (!array_buffer) {
            client_->DidFetchDataLoadFailed();
            return;
          }
          client_->DidFetchDataLoadedArrayBuffer(array_buffer);
          return;
        }
        case BytesConsumer::Result::kError:
          client_->DidFetchDataLoadFailed();
          return;
      }
    }
  }

  String DebugName() const override { return "FetchDataLoaderAsArrayBuffer"; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
  }

 private:
  // Appending empty data is not allowed. Returns false upon buffer overlow.
  bool Append(const char* data, wtf_size_t length) {
    DCHECK_GT(length, 0u);
    buffer_->Append(data, length);
    if (buffer_->size() >
        static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
      return false;
    }
    return true;
  }

  // Builds a DOMArrayBuffer from the received bytes.
  DOMArrayBuffer* BuildArrayBuffer() {
    DOMArrayBuffer* result = DOMArrayBuffer::CreateUninitializedOrNull(
        base::checked_cast<unsigned>(buffer_->size()), 1);
    // Handle a failed allocation.
    if (!result) {
      return result;
    }
    char* data = reinterpret_cast<char*>(result->Data());
    for (const auto& span : *buffer_) {
      memcpy(data, span.data(), span.size());
      data += span.size();
    }
    buffer_->Clear();
    return result;
  }

  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;

  scoped_refptr<SharedBuffer> buffer_;
};

class FetchDataLoaderAsFailure final : public FetchDataLoader,
                                       public BytesConsumer::Client {
 public:
  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!client_);
    DCHECK(!consumer_);
    client_ = client;
    consumer_ = consumer;
    consumer_->SetClient(this);
    OnStateChange();
  }

  void OnStateChange() override {
    while (true) {
      const char* buffer;
      size_t available;
      auto result = consumer_->BeginRead(&buffer, &available);
      if (result == BytesConsumer::Result::kShouldWait)
        return;
      if (result == BytesConsumer::Result::kOk)
        result = consumer_->EndRead(available);
      switch (result) {
        case BytesConsumer::Result::kOk:
          break;
        case BytesConsumer::Result::kShouldWait:
          NOTREACHED_IN_MIGRATION();
          return;
        case BytesConsumer::Result::kDone:
        case BytesConsumer::Result::kError:
          client_->DidFetchDataLoadFailed();
          return;
      }
    }
  }

  String DebugName() const override { return "FetchDataLoaderAsFailure"; }

  void Cancel() override { consumer_->Cancel(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
  }

 private:
  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;
};

class FetchDataLoaderAsFormData final : public FetchDataLoader,
                                        public BytesConsumer::Client,
                                        public MultipartParser::Client {
 public:
  explicit FetchDataLoaderAsFormData(const String& multipart_boundary)
      : multipart_boundary_(multipart_boundary) {}

  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!client_);
    DCHECK(!consumer_);
    DCHECK(!form_data_);
    DCHECK(!multipart_parser_);

    StringUTF8Adaptor multipart_boundary_utf8(multipart_boundary_);
    Vector<char> multipart_boundary_vector;
    multipart_boundary_vector.AppendSpan(base::span(multipart_boundary_utf8));

    client_ = client;
    form_data_ = MakeGarbageCollected<FormData>();
    multipart_parser_ = MakeGarbageCollected<MultipartParser>(
        std::move(multipart_boundary_vector), this);
    consumer_ = consumer;
    consumer_->SetClient(this);
    OnStateChange();
  }

  void OnStateChange() override {
    while (true) {
      const char* buffer;
      size_t available;
      auto result = consumer_->BeginRead(&buffer, &available);
      if (result == BytesConsumer::Result::kShouldWait)
        return;
      if (result == BytesConsumer::Result::kOk) {
        const bool buffer_appended =
            multipart_parser_->AppendData(base::span(buffer, available));
        const bool multipart_receive_failed = multipart_parser_->IsCancelled();
        result = consumer_->EndRead(available);
        if (!buffer_appended || multipart_receive_failed) {
          // No point in reading any more as the input is invalid.
          consumer_->Cancel();
          client_->DidFetchDataLoadFailed();
          return;
        }
      }
      switch (result) {
        case BytesConsumer::Result::kOk:
          break;
        case BytesConsumer::Result::kShouldWait:
          NOTREACHED_IN_MIGRATION();
          return;
        case BytesConsumer::Result::kDone:
          if (multipart_parser_->Finish()) {
            DCHECK(!multipart_parser_->IsCancelled());
            client_->DidFetchDataLoadedFormData(form_data_);
          } else {
            client_->DidFetchDataLoadFailed();
          }
          return;
        case BytesConsumer::Result::kError:
          client_->DidFetchDataLoadFailed();
          return;
      }
    }
  }

  String DebugName() const override { return "FetchDataLoaderAsFormData"; }

  void Cancel() override {
    consumer_->Cancel();
    multipart_parser_->Cancel();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    visitor->Trace(form_data_);
    visitor->Trace(multipart_parser_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
    MultipartParser::Client::Trace(visitor);
  }

 private:
  void PartHeaderFieldsInMultipartReceived(
      const HTTPHeaderMap& header_fields) override {
    if (!current_entry_.Initialize(header_fields))
      multipart_parser_->Cancel();
  }

  void PartDataInMultipartReceived(base::span<const char> bytes) override {
    if (!current_entry_.AppendBytes(bytes)) {
      multipart_parser_->Cancel();
    }
  }

  void PartDataInMultipartFullyReceived() override {
    if (!current_entry_.Finish(form_data_))
      multipart_parser_->Cancel();
  }

  class Entry {
   public:
    bool Initialize(const HTTPHeaderMap& header_fields) {
      const ParsedContentDisposition disposition(
          header_fields.Get(http_names::kContentDisposition));
      const String disposition_type = disposition.Type();
      filename_ = disposition.Filename();
      name_ = disposition.ParameterValueForName("name");
      blob_data_.reset();
      string_builder_.reset();
      if (disposition_type != "form-data" || name_.IsNull())
        return false;
      if (!filename_.IsNull()) {
        blob_data_ = std::make_unique<BlobData>();
        const AtomicString& content_type =
            header_fields.Get(http_names::kContentType);
        blob_data_->SetContentType(
            content_type.IsNull() ? AtomicString("text/plain") : content_type);
      } else {
        if (!string_decoder_) {
          string_decoder_ = std::make_unique<TextResourceDecoder>(
              TextResourceDecoderOptions::CreateUTF8DecodeWithoutBOM());
        }
        string_builder_ = std::make_unique<StringBuilder>();
      }
      return true;
    }

    bool AppendBytes(base::span<const char> chars) {
      if (blob_data_)
        blob_data_->AppendBytes(base::as_bytes(chars));
      if (string_builder_) {
        string_builder_->Append(string_decoder_->Decode(chars));
        if (string_decoder_->SawError())
          return false;
      }
      return true;
    }

    bool Finish(FormData* form_data) {
      if (blob_data_) {
        DCHECK(!string_builder_);
        const auto size = blob_data_->length();
        auto* file = MakeGarbageCollected<File>(
            filename_, std::nullopt,
            BlobDataHandle::Create(std::move(blob_data_), size));
        form_data->append(name_, file, filename_);
        return true;
      }
      DCHECK(!blob_data_);
      DCHECK(string_builder_);
      string_builder_->Append(string_decoder_->Flush());
      if (string_decoder_->SawError())
        return false;
      form_data->append(name_, string_builder_->ToString());
      return true;
    }

   private:
    std::unique_ptr<BlobData> blob_data_;
    String filename_;
    String name_;
    std::unique_ptr<StringBuilder> string_builder_;
    std::unique_ptr<TextResourceDecoder> string_decoder_;
  };

  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;
  Member<FormData> form_data_;
  Member<MultipartParser> multipart_parser_;

  Entry current_entry_;
  String multipart_boundary_;
};

class FetchDataLoaderAsString final : public FetchDataLoader,
                                      public BytesConsumer::Client {
 public:
  explicit FetchDataLoaderAsString(const TextResourceDecoderOptions& options)
      : decoder_options_(options) {}

  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!client_);
    DCHECK(!decoder_);
    DCHECK(!consumer_);
    client_ = client;
    decoder_ = std::make_unique<TextResourceDecoder>(decoder_options_);
    consumer_ = consumer;
    consumer_->SetClient(this);
    OnStateChange();
  }

  void OnStateChange() override {
    while (true) {
      const char* buffer;
      size_t available;
      auto result = consumer_->BeginRead(&buffer, &available);
      if (result == BytesConsumer::Result::kShouldWait)
        return;
      if (result == BytesConsumer::Result::kOk) {
        if (available > 0)
          builder_.Append(decoder_->Decode(base::span(buffer, available)));
        result = consumer_->EndRead(available);
      }
      switch (result) {
        case BytesConsumer::Result::kOk:
          break;
        case BytesConsumer::Result::kShouldWait:
          NOTREACHED_IN_MIGRATION();
          return;
        case BytesConsumer::Result::kDone:
          builder_.Append(decoder_->Flush());
          client_->DidFetchDataLoadedString(builder_.ToString());
          return;
        case BytesConsumer::Result::kError:
          client_->DidFetchDataLoadFailed();
          return;
      }
    }
  }

  String DebugName() const override { return "FetchDataLoaderAsString"; }

  void Cancel() override { consumer_->Cancel(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
  }

 private:
  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;

  std::unique_ptr<TextResourceDecoder> decoder_;
  TextResourceDecoderOptions decoder_options_;
  StringBuilder builder_;
};

class FetchDataLoaderAsDataPipe final : public FetchDataLoader,
                                        public BytesConsumer::Client {
  USING_PRE_FINALIZER(FetchDataLoaderAsDataPipe, Dispose);

 public:
  explicit FetchDataLoaderAsDataPipe(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : data_pipe_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                           task_runner),
        data_pipe_close_watcher_(FROM_HERE,
                                 mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
                                 std::move(task_runner)) {}
  ~FetchDataLoaderAsDataPipe() override = default;

  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!client_);
    DCHECK(!consumer_);

    client_ = client;
    consumer_ = consumer;
    consumer_->SetClient(this);

    // First, try to drain the underlying mojo::DataPipe from the consumer
    // directly.  If this succeeds, all we need to do here is watch for
    // the pipe to be closed to signal completion.
    mojo::ScopedDataPipeConsumerHandle pipe_consumer =
        consumer->DrainAsDataPipe();
    if (!pipe_consumer.is_valid()) {
      // If we cannot drain the pipe from the consumer then we must copy
      // data from the consumer into a new pipe.
      MojoCreateDataPipeOptions options;
      options.struct_size = sizeof(MojoCreateDataPipeOptions);
      options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
      options.element_num_bytes = 1;
      // Use the default pipe capacity since we don't know the total data
      // size to target.
      options.capacity_num_bytes = 0;

      MojoResult rv =
          mojo::CreateDataPipe(&options, out_data_pipe_, pipe_consumer);
      if (rv != MOJO_RESULT_OK) {
        StopInternal();
        client_->DidFetchDataLoadFailed();
        return;
      }
      DCHECK(out_data_pipe_.is_valid());

      data_pipe_watcher_.Watch(
          out_data_pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
          WTF::BindRepeating(&FetchDataLoaderAsDataPipe::OnWritable,
                             WrapWeakPersistent(this)));
      data_pipe_close_watcher_.Watch(
          out_data_pipe_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
          MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
          WTF::BindRepeating(&FetchDataLoaderAsDataPipe::OnPeerClosed,
                             WrapWeakPersistent(this)));

      data_pipe_watcher_.ArmOrNotify();
      data_pipe_close_watcher_.ArmOrNotify();
    }

    // Give the resulting pipe consumer handle to the client.
    DCHECK(pipe_consumer.is_valid());
    client_->DidFetchDataStartedDataPipe(std::move(pipe_consumer));

    // Its possible that the consumer changes state immediately after
    // calling DrainDataPipe.  In this case we call OnStateChange()
    // to process the new state.
    if (consumer->GetPublicState() !=
        BytesConsumer::PublicState::kReadableOrWaiting)
      OnStateChange();
  }

  void OnPeerClosed(MojoResult result, const mojo::HandleSignalsState& state) {
    StopInternal();
    client_->DidFetchDataLoadFailed();
  }

  void OnWritable(MojoResult) { OnStateChange(); }

  // Implements BytesConsumer::Client.
  void OnStateChange() override {
    bool should_wait = false;
    while (!should_wait) {
      const char* buffer;
      size_t available;
      auto result = consumer_->BeginRead(&buffer, &available);
      if (result == BytesConsumer::Result::kShouldWait)
        return;
      if (result == BytesConsumer::Result::kOk) {
        // SAFETY: `BeginRead` promises to return a valid pointer and size in
        // the `kOk` case.
        base::span<const char> span =
            UNSAFE_BUFFERS(base::span(buffer, available));
        if (span.empty()) {
          result = consumer_->EndRead(0);
        } else {
          size_t actually_written_bytes = 0;
          MojoResult mojo_result = out_data_pipe_->WriteData(
              base::as_bytes(span), MOJO_WRITE_DATA_FLAG_NONE,
              actually_written_bytes);
          if (mojo_result == MOJO_RESULT_OK) {
            result = consumer_->EndRead(actually_written_bytes);
          } else if (mojo_result == MOJO_RESULT_SHOULD_WAIT) {
            result = consumer_->EndRead(0);
            should_wait = true;
            data_pipe_watcher_.ArmOrNotify();
          } else {
            result = consumer_->EndRead(0);
            StopInternal();
            client_->DidFetchDataLoadFailed();
            return;
          }
        }
      }
      switch (result) {
        case BytesConsumer::Result::kOk:
          break;
        case BytesConsumer::Result::kShouldWait:
          NOTREACHED_IN_MIGRATION();
          return;
        case BytesConsumer::Result::kDone:
          StopInternal();
          client_->DidFetchDataLoadedDataPipe();
          return;
        case BytesConsumer::Result::kError:
          StopInternal();
          client_->DidFetchDataLoadFailed();
          return;
      }
    }
  }

  String DebugName() const override { return "FetchDataLoaderAsDataPipe"; }

  void Cancel() override { StopInternal(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
  }

 private:
  void StopInternal() {
    consumer_->Cancel();
    Dispose();
  }

  void Dispose() {
    data_pipe_watcher_.Cancel();
    data_pipe_close_watcher_.Cancel();
    out_data_pipe_.reset();
  }

  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;

  mojo::ScopedDataPipeProducerHandle out_data_pipe_;
  mojo::SimpleWatcher data_pipe_watcher_;
  mojo::SimpleWatcher data_pipe_close_watcher_;
};

}  // namespace

FetchDataLoader* FetchDataLoader::CreateLoaderAsBlobHandle(
    const String& mime_type,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return MakeGarbageCollected<FetchDataLoaderAsBlobHandle>(
      mime_type, std::move(task_runner));
}

FetchDataLoader* FetchDataLoader::CreateLoaderAsArrayBuffer() {
  return MakeGarbageCollected<FetchDataLoaderAsArrayBuffer>();
}

FetchDataLoader* FetchDataLoader::CreateLoaderAsFailure() {
  return MakeGarbageCollected<FetchDataLoaderAsFailure>();
}

FetchDataLoader* FetchDataLoader::CreateLoaderAsFormData(
    const String& multipartBoundary) {
  return MakeGarbageCollected<FetchDataLoaderAsFormData>(multipartBoundary);
}

FetchDataLoader* FetchDataLoader::CreateLoaderAsString(
    const TextResourceDecoderOptions& options) {
  return MakeGarbageCollected<FetchDataLoaderAsString>(options);
}

FetchDataLoader* FetchDataLoader::CreateLoaderAsDataPipe(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return MakeGarbageCollected<FetchDataLoaderAsDataPipe>(
      std::move(task_runner));
}

}  // namespace blink
