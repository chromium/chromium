// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"

#include "base/debug/dump_without_crashing.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"
#include "third_party/blink/renderer/core/fileapi/file_backed_blob_factory_dispatcher.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

enum class ConsumerType {
  // Has only data elements.
  kDataOnly,
  // Can have data, file, and blob elements (no data pipes).
  kDataAndEncodedFileOrBlob,
  // Can have all data types.
  kUniversal,
};

ConsumerType GetConsumerType(const EncodedFormData* form_data) {
  ConsumerType type = ConsumerType::kDataOnly;
  for (const auto& element : form_data->Elements()) {
    switch (element.type_) {
      case FormDataElement::kData:
        break;
      case FormDataElement::kEncodedFile:
      case FormDataElement::kEncodedBlob:
        type = ConsumerType::kDataAndEncodedFileOrBlob;
        break;
      case FormDataElement::kDataPipe:
        return ConsumerType::kUniversal;
    }
  }
  return type;
}

class DataOnlyBytesConsumer : public BytesConsumer {
 public:
  explicit DataOnlyBytesConsumer(scoped_refptr<EncodedFormData> form_data)
      : form_data_(std::move(form_data)) {
    DCHECK_EQ(ConsumerType::kDataOnly, GetConsumerType(form_data_.get()));
  }

  // BytesConsumer implementation
  Result BeginRead(base::span<const char>& buffer) override {
    buffer = {};
    if (form_data_) {
      form_data_->Flatten(flatten_form_data_);
      form_data_ = nullptr;
      DCHECK_EQ(flatten_form_data_offset_, 0u);
    }
    if (flatten_form_data_offset_ == flatten_form_data_.size())
      return Result::kDone;
    buffer = base::span(flatten_form_data_).subspan(flatten_form_data_offset_);
    return Result::kOk;
  }
  Result EndRead(size_t read_size) override {
    DCHECK(!form_data_);
    DCHECK_LE(flatten_form_data_offset_ + read_size, flatten_form_data_.size());
    flatten_form_data_offset_ += read_size;
    if (flatten_form_data_offset_ == flatten_form_data_.size()) {
      state_ = PublicState::kClosed;
      return Result::kDone;
    }
    return Result::kOk;
  }
  scoped_refptr<BlobDataHandle> DrainAsBlobDataHandle(
      BlobSizePolicy policy) override {
    if (!form_data_)
      return nullptr;

    Vector<char> data;
    form_data_->Flatten(data);
    form_data_ = nullptr;
    auto blob_data = std::make_unique<BlobData>();
    blob_data->AppendBytes(base::as_byte_span(data));
    auto length = blob_data->length();
    state_ = PublicState::kClosed;
    return BlobDataHandle::Create(std::move(blob_data), length);
  }
  scoped_refptr<EncodedFormData> DrainAsFormData() override {
    if (!form_data_)
      return nullptr;

    state_ = PublicState::kClosed;
    return std::move(form_data_);
  }
  void SetClient(BytesConsumer::Client* client) override { DCHECK(client); }
  void ClearClient() override {}
  void Cancel() override {
    state_ = PublicState::kClosed;
    form_data_ = nullptr;
    flatten_form_data_.clear();
    flatten_form_data_offset_ = 0;
  }
  PublicState GetPublicState() const override { return state_; }
  Error GetError() const override { NOTREACHED(); }
  String DebugName() const override { return "DataOnlyBytesConsumer"; }

 private:
  // Either one of |form_data_| and |flatten_form_data_| is usable at a time.
  scoped_refptr<EncodedFormData> form_data_;
  Vector<char> flatten_form_data_;
  size_t flatten_form_data_offset_ = 0;
  PublicState state_ = PublicState::kReadableOrWaiting;
};

class DataAndEncodedFileOrBlobBytesConsumer final : public BytesConsumer {
 public:
  DataAndEncodedFileOrBlobBytesConsumer(
      ExecutionContext* execution_context,
      scoped_refptr<EncodedFormData> form_data,
      BytesConsumer* consumer_for_testing)
      : form_data_(std::move(form_data)) {
    DCHECK_EQ(ConsumerType::kDataAndEncodedFileOrBlob,
              GetConsumerType(form_data_.get()));
    CHECK(form_data_->Boundary().data());
    if (consumer_for_testing) {
      blob_bytes_consumer_ = consumer_for_testing;
      return;
    }

    auto blob_data = std::make_unique<BlobData>();
    for (const auto& element : form_data_->Elements()) {
      switch (element.type_) {
        case FormDataElement::kData:
          blob_data->AppendBytes(base::as_byte_span(element.data_));
          break;
        case FormDataElement::kEncodedFile: {
          auto file_length = element.file_length_;
          if (file_length < 0) {
            if (!GetFileSize(element.filename_, *execution_context,
                             file_length)) {
              form_data_ = nullptr;
              blob_bytes_consumer_ = BytesConsumer::CreateErrored(
                  Error("Cannot determine a file size"));
              return;
            }
          }
          blob_data->AppendBlob(
              BlobDataHandle::CreateForFile(
                  FileBackedBlobFactoryDispatcher::GetFileBackedBlobFactory(
                      execution_context),
                  element.filename_, element.file_start_, file_length,
                  element.expected_file_modification_time_,
                  /*content_type=*/""),
              0, file_length);
          break;
        }
        case FormDataElement::kEncodedBlob:
          if (element.blob_data_handle_) {
            blob_data->AppendBlob(element.blob_data_handle_, 0,
                                  element.blob_data_handle_->size());
          }
          break;
        case FormDataElement::kDataPipe:
          LOG(ERROR) << "This consumer can't handle data pipes.";
          base::debug::DumpWithoutCrashing();
          break;
      }
    }
    blob_data->SetContentType(form_data_->FormatContentTypeWithBoundary());
    auto size = blob_data->length();
    blob_bytes_consumer_ = MakeGarbageCollected<BlobBytesConsumer>(
        execution_context, BlobDataHandle::Create(std::move(blob_data), size));
  }

  // BytesConsumer implementation
  Result BeginRead(base::span<const char>& buffer) override {
    form_data_ = nullptr;
    // Delegate the operation to the underlying consumer. This relies on
    // the fact that we appropriately notify the draining information to
    // the underlying consumer.
    return blob_bytes_consumer_->BeginRead(buffer);
  }
  Result EndRead(size_t read_size) override {
    return blob_bytes_consumer_->EndRead(read_size);
  }
  scoped_refptr<BlobDataHandle> DrainAsBlobDataHandle(
      BlobSizePolicy policy) override {
    LOG(ERROR) << "DrainAsBlobDataHandle";
    scoped_refptr<BlobDataHandle> handle =
        blob_bytes_consumer_->DrainAsBlobDataHandle(policy);
    if (handle) {
      form_data_ = nullptr;
    }
    return handle;
  }
  scoped_refptr<EncodedFormData> DrainAsFormData() override {
    if (!form_data_) {
      return nullptr;
    }
    blob_bytes_consumer_->Cancel();
    return std::move(form_data_);
  }
  void SetClient(BytesConsumer::Client* client) override {
    blob_bytes_consumer_->SetClient(client);
  }
  void ClearClient() override { blob_bytes_consumer_->ClearClient(); }
  void Cancel() override {
    form_data_ = nullptr;
    blob_bytes_consumer_->Cancel();
  }
  PublicState GetPublicState() const override {
    return blob_bytes_consumer_->GetPublicState();
  }
  Error GetError() const override { return blob_bytes_consumer_->GetError(); }
  String DebugName() const override {
    return "DataAndEncodedFileOrBlobBytesConsumer";
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(blob_bytes_consumer_);
    BytesConsumer::Trace(visitor);
  }

 private:
  scoped_refptr<EncodedFormData> form_data_;
  Member<BytesConsumer> blob_bytes_consumer_;
};

// BytesConsumer reading from network::mojom::blink::DataPipeGetter.
// This is an intermediate class used by
// DataAndDataPipeBytesConsumerDataAndDataPipeBytesConsumer.
class DataPipeGetterConsumer : public BytesConsumer {
 public:
  static DataPipeGetterConsumer* Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      network::mojom::blink::DataPipeGetter* data_pipe_getter) {
    mojo::ScopedDataPipeProducerHandle pipe_producer_handle;
    mojo::ScopedDataPipeConsumerHandle pipe_consumer_handle;
    MojoResult rv = mojo::CreateDataPipe(nullptr, pipe_producer_handle,
                                         pipe_consumer_handle);
    if (rv != MOJO_RESULT_OK) {
      return nullptr;
    }
    DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;
    DataPipeBytesConsumer* data_pipe_consumer =
        MakeGarbageCollected<DataPipeBytesConsumer>(
            std::move(task_runner), std::move(pipe_consumer_handle),
            &completion_notifier);
    DataPipeGetterConsumer* consumer =
        MakeGarbageCollected<DataPipeGetterConsumer>(data_pipe_consumer,
                                                     completion_notifier);

    data_pipe_getter->Read(
        std::move(pipe_producer_handle),
        BindOnce(&DataPipeGetterConsumer::DataPipeGetterCallback,
                 WrapWeakPersistent(consumer)));
    return consumer;
  }

  DataPipeGetterConsumer(
      DataPipeBytesConsumer* data_pipe_consumer,
      DataPipeBytesConsumer::CompletionNotifier* completion_notifier)
      : data_pipe_consumer_(data_pipe_consumer),
        completion_notifier_(completion_notifier) {
    CHECK(data_pipe_consumer_);
    CHECK(completion_notifier_);
  }

  Result BeginRead(base::span<const char>& buffer) override {
    return data_pipe_consumer_->BeginRead(buffer);
  }
  Result EndRead(size_t read_size) override {
    return data_pipe_consumer_->EndRead(read_size);
  }
  void SetClient(BytesConsumer::Client* client) override {
    data_pipe_consumer_->SetClient(client);
  }
  void ClearClient() override { data_pipe_consumer_->ClearClient(); }

  void Cancel() override { data_pipe_consumer_->Cancel(); }
  PublicState GetPublicState() const override {
    return data_pipe_consumer_->GetPublicState();
  }
  Error GetError() const override { return data_pipe_consumer_->GetError(); }
  String DebugName() const override { return "DataPipeGetterConsumer"; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(data_pipe_consumer_);
    visitor->Trace(completion_notifier_);
    BytesConsumer::Trace(visitor);
  }

 private:
  void DataPipeGetterCallback(int32_t status, uint64_t size) {
    CHECK(completion_notifier_);
    if (status == 0) {
      // 0 is net::OK.
      completion_notifier_->SignalComplete();
    } else {
      completion_notifier_->SignalError(Error("error"));
    }
  }

  Member<DataPipeBytesConsumer> data_pipe_consumer_;
  Member<DataPipeBytesConsumer::CompletionNotifier> completion_notifier_;
};

class UniversalBytesConsumer final : public BytesConsumer {
 public:
  UniversalBytesConsumer(ExecutionContext* execution_context,
                         EncodedFormData* form_data)
      : execution_context_(execution_context) {
    // Make a copy in case |form_data| will mutate while we read it. Copy()
    // works fine; we don't need to DeepCopy() the data and data pipe getter:
    // data is just a Vector<char> and data pipe getter can be shared.
    form_data_ = form_data->Copy();
    form_data_->SetBoundary(FormDataEncoder::GenerateUniqueBoundaryString());
    iter_ = form_data_->MutableElements().CheckedBegin();
  }

  Result BeginRead(base::span<const char>& buffer) override {
    buffer = {};
    if (state_ == PublicState::kClosed)
      return Result::kDone;
    if (state_ == PublicState::kErrored)
      return Result::kError;

    if (iter_ == form_data_->MutableElements().CheckedEnd()) {
      Close();
      return Result::kDone;
    }
    // Create correspondending bytes consumer if there isn't one yet.
    if (!bytes_consumer_) {
      switch (iter_->type_) {
        case FormDataElement::kData: {
          scoped_refptr<EncodedFormData> simple_data =
              EncodedFormData::Create(iter_->data_);
          bytes_consumer_ = MakeGarbageCollected<DataOnlyBytesConsumer>(
              std::move(simple_data));
          break;
        }
        case FormDataElement::kEncodedFile:
        case FormDataElement::kEncodedBlob: {
          scoped_refptr<EncodedFormData> form_data = EncodedFormData::Create();
          form_data->SetBoundary(form_data_->Boundary());
          form_data->MutableElements().push_back(std::move(*iter_));
          bytes_consumer_ =
              MakeGarbageCollected<DataAndEncodedFileOrBlobBytesConsumer>(
                  execution_context_, std::move(form_data), nullptr);
          break;
        }
        case FormDataElement::kDataPipe: {
          bytes_consumer_ = DataPipeGetterConsumer::Create(
              execution_context_->GetTaskRunner(TaskType::kNetworking),
              iter_->data_pipe_getter_->GetDataPipeGetter());
          if (!bytes_consumer_) {
            return Result::kError;
          }
          break;
        }
      }

      if (client_) {
        bytes_consumer_->SetClient(client_);
      }
    }
    CHECK(bytes_consumer_);
    // Read from the bytes consumer.
    Result result = bytes_consumer_->BeginRead(buffer);
    if (result == Result::kError) {
      SetError();
      return Result::kError;
    }
    // If done, continue to the next element.
    if (result == Result::kDone) {
      // No buffer should be read in this case.
      DCHECK(buffer.empty());
      bytes_consumer_ = nullptr;
      ++iter_;
      return BeginRead(buffer);
    }
    return result;
  }

  Result EndRead(size_t read_size) override {
    if (state_ == PublicState::kClosed)
      return Result::kDone;
    if (state_ == PublicState::kErrored)
      return Result::kError;

    if (bytes_consumer_) {
      Result result = bytes_consumer_->EndRead(read_size);
      if (result == Result::kError) {
        SetError();
        return Result::kError;
      }
      // Even if this consumer is done, there may still be more elements, so
      // return Ok.
      DCHECK(result == Result::kOk || result == Result::kDone);
      return Result::kOk;
    }

    NOTREACHED() << "No consumer. BeginRead() was not called?";
  }

  scoped_refptr<EncodedFormData> DrainAsFormData() override {
    if (state_ == PublicState::kClosed || state_ == PublicState::kErrored)
      return nullptr;
    // According to the DrainAsFormData() contract, we can only return bytes
    // that haven't already been read. So if reading has already started,
    // give up and return null.
    if (bytes_consumer_) {
      return nullptr;
    }
    Close();
    return std::move(form_data_);
  }

  void SetClient(Client* client) override {
    DCHECK(!client_);
    DCHECK(client);
    client_ = client;
    if (bytes_consumer_) {
      bytes_consumer_->SetClient(client_);
    }
  }

  void ClearClient() override {
    client_ = nullptr;
    if (bytes_consumer_) {
      bytes_consumer_->ClearClient();
    }
  }

  void Cancel() override {
    if (state_ == PublicState::kClosed || state_ == PublicState::kErrored)
      return;
    if (bytes_consumer_) {
      bytes_consumer_->Cancel();
    }
    Close();
  }

  PublicState GetPublicState() const override { return state_; }

  Error GetError() const override {
    DCHECK_EQ(state_, PublicState::kErrored);
    return error_;
  }

  String DebugName() const override { return "DataAndDataPipeBytesConsumer"; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(execution_context_);
    visitor->Trace(client_);
    visitor->Trace(bytes_consumer_);
    BytesConsumer::Trace(visitor);
  }

 private:
  void Close() {
    if (state_ == PublicState::kClosed)
      return;
    DCHECK_EQ(state_, PublicState::kReadableOrWaiting);
    state_ = PublicState::kClosed;
    ClearClient();
    if (bytes_consumer_) {
      bytes_consumer_->Cancel();
      bytes_consumer_ = nullptr;
    }
  }

  void SetError() {
    if (state_ == PublicState::kErrored)
      return;
    DCHECK_EQ(state_, PublicState::kReadableOrWaiting);
    state_ = PublicState::kErrored;
    error_ = Error("error");
    ClearClient();
    bytes_consumer_ = nullptr;
  }

  Member<ExecutionContext> execution_context_;
  PublicState state_ = PublicState::kReadableOrWaiting;
  scoped_refptr<EncodedFormData> form_data_;
  base::CheckedContiguousIterator<Vector<FormDataElement>::ValueType> iter_;
  Error error_;
  Member<BytesConsumer::Client> client_;
  Member<BytesConsumer> bytes_consumer_;
};

}  // namespace

FormDataBytesConsumer::FormDataBytesConsumer(const String& string)
    : impl_(MakeGarbageCollected<DataOnlyBytesConsumer>(EncodedFormData::Create(
          Utf8Encoding().Encode(string,
                                UnencodableHandling::kNoUnencodables)))) {}

FormDataBytesConsumer::FormDataBytesConsumer(DOMArrayBuffer* buffer)
    : FormDataBytesConsumer(buffer->ByteSpan()) {}

FormDataBytesConsumer::FormDataBytesConsumer(DOMArrayBufferView* view)
    : FormDataBytesConsumer(view->ByteSpan()) {}

FormDataBytesConsumer::FormDataBytesConsumer(SegmentedBuffer&& buffer)
    : impl_(MakeGarbageCollected<DataOnlyBytesConsumer>(
          EncodedFormData::Create(std::move(buffer)))) {}

FormDataBytesConsumer::FormDataBytesConsumer(base::span<const uint8_t> bytes)
    : impl_(MakeGarbageCollected<DataOnlyBytesConsumer>(
          EncodedFormData::Create(bytes))) {}

FormDataBytesConsumer::FormDataBytesConsumer(
    ExecutionContext* execution_context,
    scoped_refptr<EncodedFormData> form_data)
    : FormDataBytesConsumer(execution_context, std::move(form_data), nullptr) {}

FormDataBytesConsumer::FormDataBytesConsumer(
    ExecutionContext* execution_context,
    scoped_refptr<EncodedFormData> form_data,
    BytesConsumer* consumer_for_testing)
    : impl_(GetImpl(execution_context,
                    std::move(form_data),
                    consumer_for_testing)) {}

// static
BytesConsumer* FormDataBytesConsumer::GetImpl(
    ExecutionContext* execution_context,
    scoped_refptr<EncodedFormData> form_data,
    BytesConsumer* consumer_for_testing) {
  DCHECK(form_data);
  const ConsumerType consumer_type = GetConsumerType(form_data.get());
  switch (consumer_type) {
    case ConsumerType::kDataOnly:
      return MakeGarbageCollected<DataOnlyBytesConsumer>(std::move(form_data));
    case ConsumerType::kDataAndEncodedFileOrBlob:
      return MakeGarbageCollected<DataAndEncodedFileOrBlobBytesConsumer>(
          execution_context, std::move(form_data), consumer_for_testing);
    case ConsumerType::kUniversal:
      return MakeGarbageCollected<UniversalBytesConsumer>(execution_context,
                                                          form_data.get());
  }
  return nullptr;
}

}  // namespace blink
