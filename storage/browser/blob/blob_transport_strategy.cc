// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_transport_strategy.h"

#include <memory>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom.h"

namespace storage {

namespace {

using MemoryStrategy = BlobMemoryController::Strategy;

// Transport strategy when no transport is needed. All Bytes elements should
// have their data embedded already.
class NoneNeededTransportStrategy : public BlobTransportStrategy {
 public:
  NoneNeededTransportStrategy(BlobDataBuilder* builder,
                              ResultCallback result_callback)
      : BlobTransportStrategy(builder, std::move(result_callback)) {}

  void AddBytesElement(
      blink::mojom::DataElementBytes* bytes,
      const mojo::Remote<blink::mojom::BytesProvider>& data) override {
    DCHECK(bytes->embedded_data);
    DCHECK_EQ(bytes->length, bytes->embedded_data->size());
    builder_->AppendData(base::make_span(*bytes->embedded_data));
  }

  void BeginTransport(
      std::vector<BlobMemoryController::FileCreationInfo>) override {
    std::move(result_callback_).Run(BlobStatus::DONE);
  }
};

// Transport strategy that requests all data as replies.
class ReplyTransportStrategy : public BlobTransportStrategy {
 public:
  ReplyTransportStrategy(BlobDataBuilder* builder,
                         ResultCallback result_callback)
      : BlobTransportStrategy(builder, std::move(result_callback)) {}

  void AddBytesElement(
      blink::mojom::DataElementBytes* bytes,
      const mojo::Remote<blink::mojom::BytesProvider>& data) override {
    BlobDataBuilder::FutureData future_data =
        builder_->AppendFutureData(bytes->length);
    // base::Unretained is safe because |this| is guaranteed (by the contract
    // that code using BlobTransportStrategy should adhere to) to outlive the
    // BytesProvider.
    requests_.push_back(base::BindOnce(
        &blink::mojom::BytesProvider::RequestAsReply,
        base::Unretained(data.get()),
        base::BindOnce(&ReplyTransportStrategy::OnReply, base::Unretained(this),
                       std::move(future_data), bytes->length)));
  }

  void BeginTransport(
      std::vector<BlobMemoryController::FileCreationInfo>) override {
    if (requests_.empty()) {
      std::move(result_callback_).Run(BlobStatus::DONE);
      return;
    }
    for (auto& request : requests_)
      std::move(request).Run();
  }

 private:
  void OnReply(BlobDataBuilder::FutureData future_data,
               size_t expected_size,
               const std::vector<uint8_t>& data) {
    if (data.size() != expected_size) {
      mojo::ReportBadMessage(
          "Invalid data size in reply to BytesProvider::RequestAsReply");
      std::move(result_callback_)
          .Run(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
      return;
    }
    bool populate_result = future_data.Populate(base::make_span(data), 0);
    DCHECK(populate_result);

    if (++num_resolved_requests_ == requests_.size())
      std::move(result_callback_).Run(BlobStatus::DONE);
  }

  std::vector<base::OnceClosure> requests_;
  size_t num_resolved_requests_ = 0;
};

// Transport strategy that requests all data as data pipes, one pipe at a time.
class DataPipeTransportStrategy : public BlobTransportStrategy {
 public:
  DataPipeTransportStrategy(BlobDataBuilder* builder,
                            ResultCallback result_callback,
                            const BlobStorageLimits& limits)
      : BlobTransportStrategy(builder, std::move(result_callback)),
        limits_(limits),
        watcher_(FROM_HERE,
                 mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
                 base::SequencedTaskRunnerHandle::Get()) {}

  void AddBytesElement(
      blink::mojom::DataElementBytes* bytes,
      const mojo::Remote<blink::mojom::BytesProvider>& data) override {
    // Split up the data in |max_bytes_data_item_size| sized chunks.
    std::vector<BlobDataBuilder::FutureData> future_data;
    for (uint64_t source_offset = 0; source_offset < bytes->length;
         source_offset += limits_.max_bytes_data_item_size) {
      future_data.push_back(builder_->AppendFutureData(std::min<uint64_t>(
          bytes->length - source_offset, limits_.max_bytes_data_item_size)));
    }
    requests_.push_back(base::BindOnce(
        &DataPipeTransportStrategy::RequestDataPipe, base::Unretained(this),
        data.get(), bytes->length, std::move(future_data)));
  }

  void BeginTransport(
      std::vector<BlobMemoryController::FileCreationInfo>) override {
    NextRequestOrDone();
  }

 private:
  void NextRequestOrDone() {
    if (requests_.empty()) {
      std::move(result_callback_).Run(BlobStatus::DONE);
      return;
    }
    auto request = std::move(requests_.front());
    requests_.pop_front();
    std::move(request).Run();
  }

  void RequestDataPipe(blink::mojom::BytesProvider* provider,
                       size_t expected_source_size,
                       std::vector<BlobDataBuilder::FutureData> future_data) {
    // TODO(mek): Determine if the overhead of creating a new SharedMemory
    // segment for each BytesProvider is too much. If it is possible solutions
    // would include somehow teaching DataPipe to reuse the SharedMemory from a
    // previous DataPipe, or simply using a single BytesProvider for all bytes
    // elements. http://crbug.com/741159
    DCHECK(!consumer_handle_.is_valid());
    mojo::ScopedDataPipeProducerHandle producer_handle;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes =
        std::min(expected_source_size, limits_.max_shared_memory_size);
    MojoResult result =
        CreateDataPipe(&options, &producer_handle, &consumer_handle_);
    if (result != MOJO_RESULT_OK) {
      DVLOG(1) << "Unable to create data pipe for blob transfer.";
      std::move(result_callback_).Run(BlobStatus::ERR_OUT_OF_MEMORY);
      return;
    }

    current_source_offset_ = 0;
    provider->RequestAsStream(std::move(producer_handle));
    watcher_.Watch(
        consumer_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
        MOJO_WATCH_CONDITION_SATISFIED,
        base::BindRepeating(&DataPipeTransportStrategy::OnDataPipeReadable,
                            base::Unretained(this), expected_source_size,
                            std::move(future_data)));
  }

  void OnDataPipeReadable(
      size_t expected_full_source_size,
      const std::vector<BlobDataBuilder::FutureData>& future_data,
      MojoResult result,
      const mojo::HandleSignalsState& state) {
    // The index of the element data should currently be written to, relative to
    // the first element of this stream (the first item in future_data).
    size_t relative_element_index =
        current_source_offset_ / limits_.max_bytes_data_item_size;
    DCHECK_LT(relative_element_index, future_data.size());
    // The offset into the current element where data should be written next.
    size_t offset_in_builder_element =
        current_source_offset_ -
        relative_element_index * limits_.max_bytes_data_item_size;

    while (true) {
      uint32_t num_bytes = 0;
      const void* source_buffer;
      MojoResult read_result = consumer_handle_->BeginReadData(
          &source_buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
      if (read_result == MOJO_RESULT_SHOULD_WAIT)
        return;
      if (read_result != MOJO_RESULT_OK) {
        // Data pipe broke before we received all the data.
        std::move(result_callback_).Run(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT);
        return;
      }

      if (current_source_offset_ + num_bytes > expected_full_source_size) {
        // Received more bytes then expected.
        std::move(result_callback_)
            .Run(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
        return;
      }

      // Only read as many bytes as can fit in current data element. Any
      // remaining bytes will be read on the next iteration of this loop.
      num_bytes =
          std::min<uint32_t>(num_bytes, limits_.max_bytes_data_item_size -
                                            offset_in_builder_element);
      base::span<uint8_t> output_buffer =
          future_data[relative_element_index].GetDataToPopulate(
              offset_in_builder_element, num_bytes);
      DCHECK(output_buffer.data());

      std::memcpy(output_buffer.data(), source_buffer, num_bytes);
      read_result = consumer_handle_->EndReadData(num_bytes);
      DCHECK_EQ(read_result, MOJO_RESULT_OK);

      current_source_offset_ += num_bytes;
      if (current_source_offset_ >= expected_full_source_size) {
        // Done with this stream, on to the next.
        // TODO(mek): Should this wait to see if more data than expected gets
        // written, instead of immediately closing the pipe?
        watcher_.Cancel();
        consumer_handle_.reset();
        NextRequestOrDone();
        return;
      }

      offset_in_builder_element += num_bytes;
      if (offset_in_builder_element >= limits_.max_bytes_data_item_size) {
        offset_in_builder_element = 0;
        relative_element_index++;
      }
    }
  }

  const BlobStorageLimits& limits_;
  base::circular_deque<base::OnceClosure> requests_;

  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  mojo::SimpleWatcher watcher_;
  // How many bytes have been read and processed so far from the current data
  // pipe.
  size_t current_source_offset_ = 0;
};

// Transport strategy that requests all data through files.
class FileTransportStrategy : public BlobTransportStrategy {
 public:
  FileTransportStrategy(BlobDataBuilder* builder,
                        ResultCallback result_callback,
                        const BlobStorageLimits& limits)
      : BlobTransportStrategy(builder, std::move(result_callback)),
        limits_(limits) {}

  void AddBytesElement(
      blink::mojom::DataElementBytes* bytes,
      const mojo::Remote<blink::mojom::BytesProvider>& data) override {
    uint64_t source_offset = 0;
    while (source_offset < bytes->length) {
      if (current_file_size_ >= limits_.max_file_size ||
          file_requests_.empty()) {
        current_file_size_ = 0;
        current_file_index_++;
        file_requests_.push_back(std::vector<Request>());
      }

      // Make sure no single file gets too big, but do use up all the available
      // space in all but the last file.
      uint64_t element_size =
          std::min(bytes->length - source_offset,
                   limits_.max_file_size - current_file_size_);
      BlobDataBuilder::FutureFile future_file = builder_->AppendFutureFile(
          current_file_size_, element_size, file_requests_.size() - 1);

      num_unresolved_requests_++;
      file_requests_.back().push_back(Request{
          data.get(), source_offset, element_size, std::move(future_file)});

      source_offset += element_size;
      current_file_size_ += element_size;
    }
  }

  void BeginTransport(
      std::vector<BlobMemoryController::FileCreationInfo> file_infos) override {
    if (file_requests_.empty()) {
      std::move(result_callback_).Run(BlobStatus::DONE);
      return;
    }
    DCHECK_EQ(file_infos.size(), file_requests_.size());
    for (size_t file_index = 0; file_index < file_requests_.size();
         ++file_index) {
      auto& requests = file_requests_[file_index];
      uint64_t file_offset = 0;
      for (size_t i = 0; i < requests.size(); ++i) {
        auto& request = requests[i];
        base::File file = i == requests.size() - 1
                              ? std::move(file_infos[file_index].file)
                              : file_infos[file_index].file.Duplicate();
        // base::Unretained is safe because |this| is guaranteed (by the
        // contract that code using BlobTransportStrategy should adhere to) to
        // outlive the BytesProvider.
        request.provider->RequestAsFile(
            request.source_offset, request.source_size, std::move(file),
            file_offset,
            base::BindOnce(&FileTransportStrategy::OnReply,
                           base::Unretained(this),
                           std::move(request.future_file),
                           file_infos[file_index].file_reference));
        file_offset += request.source_size;
      }
    }
  }

 private:
  void OnReply(BlobDataBuilder::FutureFile future_file,
               scoped_refptr<ShareableFileReference> file_reference,
               base::Optional<base::Time> time_file_modified) {
    if (!time_file_modified) {
      // Writing to the file failed in the renderer.
      std::move(result_callback_).Run(BlobStatus::ERR_FILE_WRITE_FAILED);
      return;
    }

    bool populate_result =
        future_file.Populate(std::move(file_reference), *time_file_modified);
    DCHECK(populate_result);

    if (--num_unresolved_requests_ == 0)
      std::move(result_callback_).Run(BlobStatus::DONE);
  }

  const BlobStorageLimits& limits_;

  // State used to assign bytes elements to individual files.
  // The index of the first file that still has available space.
  size_t current_file_index_ = 0;
  // How big the current file already is.
  uint64_t current_file_size_ = 0;

  struct Request {
    // The BytesProvider to request this particular bit of data from.
    blink::mojom::BytesProvider* provider;
    // Offset into the BytesProvider of the data to request.
    uint64_t source_offset;
    // Size of the bytes to request.
    uint64_t source_size;
    // Future file the data should be populated into.
    BlobDataBuilder::FutureFile future_file;
  };
  // For each file, a list of requests involving that file.
  std::vector<std::vector<Request>> file_requests_;

  size_t num_unresolved_requests_ = 0;
};

}  // namespace

// static
std::unique_ptr<BlobTransportStrategy> BlobTransportStrategy::Create(
    MemoryStrategy strategy,
    BlobDataBuilder* builder,
    ResultCallback result_callback,
    const BlobStorageLimits& limits) {
  switch (strategy) {
    case MemoryStrategy::NONE_NEEDED:
      return std::make_unique<NoneNeededTransportStrategy>(
          builder, std::move(result_callback));
    case MemoryStrategy::IPC:
      return std::make_unique<ReplyTransportStrategy>(
          builder, std::move(result_callback));
    case MemoryStrategy::SHARED_MEMORY:
      return std::make_unique<DataPipeTransportStrategy>(
          builder, std::move(result_callback), limits);
    case MemoryStrategy::FILE:
      return std::make_unique<FileTransportStrategy>(
          builder, std::move(result_callback), limits);
    case MemoryStrategy::TOO_LARGE:
      NOTREACHED();
  }
  NOTREACHED();
  return nullptr;
}

BlobTransportStrategy::~BlobTransportStrategy() = default;

BlobTransportStrategy::BlobTransportStrategy(BlobDataBuilder* builder,
                                             ResultCallback result_callback)
    : builder_(builder), result_callback_(std::move(result_callback)) {}

}  // namespace storage
