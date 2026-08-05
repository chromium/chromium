// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_transport_strategy.h"

#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/circular_deque.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
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
    builder_->AppendData(base::span(*bytes->embedded_data));
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
    bool populate_result = future_data.Populate(base::span(data), 0);
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
                 base::SequencedTaskRunner::GetCurrentDefault()) {}

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
        CreateDataPipe(&options, producer_handle, consumer_handle_);
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
      base::span<const uint8_t> source_buffer;
      MojoResult read_result = consumer_handle_->BeginReadData(
          MOJO_READ_DATA_FLAG_NONE, source_buffer);
      if (read_result == MOJO_RESULT_SHOULD_WAIT)
        return;
      if (read_result != MOJO_RESULT_OK) {
        // Data pipe broke before we received all the data.
        std::move(result_callback_).Run(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT);
        return;
      }

      if (current_source_offset_ + source_buffer.size() >
          expected_full_source_size) {
        // Received more bytes then expected.
        std::move(result_callback_)
            .Run(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
        return;
      }

      // Only read as many bytes as can fit in current data element. Any
      // remaining bytes will be read on the next iteration of this loop.
      size_t num_bytes =
          std::min(source_buffer.size(), limits_.max_bytes_data_item_size -
                                             offset_in_builder_element);
      base::span<uint8_t> output_buffer =
          future_data[relative_element_index].GetDataToPopulate(
              offset_in_builder_element, num_bytes);
      DCHECK(output_buffer.data());

      output_buffer.copy_prefix_from(source_buffer.first(num_bytes));
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

  const BlobStorageLimits limits_;
  base::circular_deque<base::OnceClosure> requests_;

  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  mojo::SimpleWatcher watcher_;
  // How many bytes have been read and processed so far from the current data
  // pipe.
  size_t current_source_offset_ = 0;
};

// Transport strategy that stores all data in page files. Bytes are streamed
// over data pipes and written to the files locally so that the page files
// remain private to this process.
class FileTransportStrategy : public BlobTransportStrategy {
 public:
  FileTransportStrategy(BlobDataBuilder* builder,
                        ResultCallback result_callback,
                        const BlobStorageLimits& limits)
      : BlobTransportStrategy(builder, std::move(result_callback)),
        limits_(limits),
        reply_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
        file_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {}

  ~FileTransportStrategy() override {
    if (!files_.empty()) {
      file_runner_->PostTask(
          FROM_HERE,
          base::BindOnce([](std::vector<base::File>) {}, std::move(files_)));
    }
  }

  void AddBytesElement(
      blink::mojom::DataElementBytes* bytes,
      const mojo::Remote<blink::mojom::BytesProvider>& data) override {
    if (bytes->length == 0) {
      return;
    }
    Element element;
    element.provider = data.get();
    element.length = bytes->length;
    uint64_t source_offset = 0;
    while (source_offset < bytes->length) {
      if (current_file_size_ >= limits_.max_file_size || file_count_ == 0) {
        current_file_size_ = 0;
        file_count_++;
      }

      // Make sure no single file gets too big, but do use up all the available
      // space in all but the last file.
      uint64_t segment_size =
          std::min(bytes->length - source_offset,
                   limits_.max_file_size - current_file_size_);
      element.future_files.push_back(builder_->AppendFutureFile(
          current_file_size_, segment_size, file_count_ - 1));
      element.segments.push_back(
          Segment{file_count_ - 1, current_file_size_, segment_size});

      source_offset += segment_size;
      current_file_size_ += segment_size;
    }
    elements_.push_back(std::move(element));
  }

  void BeginTransport(
      std::vector<BlobMemoryController::FileCreationInfo> file_infos) override {
    if (elements_.empty()) {
      std::move(result_callback_).Run(BlobStatus::DONE);
      return;
    }
    DCHECK_EQ(file_infos.size(), file_count_);
    file_references_.reserve(file_infos.size());
    files_.reserve(file_infos.size());
    for (auto& info : file_infos) {
      file_references_.push_back(std::move(info.file_reference));
      files_.push_back(std::move(info.file));
    }
    NextElementOrDone();
  }

 private:
  // A contiguous run of bytes from one element to be written to one file.
  struct Segment {
    size_t file_index;
    uint64_t file_offset;
    uint64_t size;
  };

  struct Element {
    raw_ptr<blink::mojom::BytesProvider> provider;
    uint64_t length = 0;
    // Segments are ordered by source offset and cover the whole element.
    std::vector<Segment> segments;
    // One entry per segment.
    std::vector<BlobDataBuilder::FutureFile> future_files;
  };

  using StreamWrittenCallback =
      base::OnceCallback<void(std::vector<base::File>, uint64_t bytes_written)>;

  // Runs on |file_runner_|. Reads from a data pipe and writes the bytes to the
  // page files according to |segments|. Self-deletes when done.
  class StreamToFilesWriter {
   public:
    static void CreateAndStart(
        std::vector<base::File> files,
        mojo::ScopedDataPipeConsumerHandle pipe,
        std::vector<Segment> segments,
        uint64_t expected_size,
        scoped_refptr<base::SequencedTaskRunner> reply_runner,
        StreamWrittenCallback callback) {
      new StreamToFilesWriter(std::move(files), std::move(pipe),
                              std::move(segments), expected_size,
                              std::move(reply_runner), std::move(callback));
    }

   private:
    StreamToFilesWriter(std::vector<base::File> files,
                        mojo::ScopedDataPipeConsumerHandle pipe,
                        std::vector<Segment> segments,
                        uint64_t expected_size,
                        scoped_refptr<base::SequencedTaskRunner> reply_runner,
                        StreamWrittenCallback callback)
        : files_(std::move(files)),
          pipe_(std::move(pipe)),
          watcher_(FROM_HERE,
                   mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                   base::SequencedTaskRunner::GetCurrentDefault()),
          segments_(std::move(segments)),
          expected_size_(expected_size),
          reply_runner_(std::move(reply_runner)),
          callback_(std::move(callback)) {
      watcher_.Watch(pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_WATCH_CONDITION_SATISFIED,
                     base::BindRepeating(&StreamToFilesWriter::OnReadable,
                                         base::Unretained(this)));
      watcher_.ArmOrNotify();
    }

    void OnReadable(MojoResult result, const mojo::HandleSignalsState& state) {
      while (bytes_written_ < expected_size_) {
        base::span<const uint8_t> buffer;
        MojoResult read_result =
            pipe_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
        if (read_result == MOJO_RESULT_SHOULD_WAIT) {
          watcher_.ArmOrNotify();
          return;
        }
        if (read_result != MOJO_RESULT_OK) {
          // The producer closed the pipe before we received all the data.
          break;
        }

        size_t consumed = 0;
        while (consumed < buffer.size() && segment_index_ < segments_.size()) {
          const Segment& segment = segments_[segment_index_];
          uint64_t remaining_in_segment = segment.size - segment_offset_;
          size_t chunk = static_cast<size_t>(std::min<uint64_t>(
              buffer.size() - consumed, remaining_in_segment));
          std::optional<size_t> written = files_[segment.file_index].Write(
              segment.file_offset + segment_offset_,
              buffer.subspan(consumed, chunk));
          if (!written.has_value() || *written != chunk) {
            pipe_->EndReadData(consumed);
            Done();
            return;
          }
          consumed += chunk;
          bytes_written_ += chunk;
          segment_offset_ += chunk;
          if (segment_offset_ == segment.size) {
            segment_index_++;
            segment_offset_ = 0;
          }
        }
        pipe_->EndReadData(consumed);
      }
      Done();
    }

    void Done() {
      reply_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_), std::move(files_),
                                    bytes_written_));
      delete this;
    }

    std::vector<base::File> files_;
    mojo::ScopedDataPipeConsumerHandle pipe_;
    mojo::SimpleWatcher watcher_;
    const std::vector<Segment> segments_;
    const uint64_t expected_size_;
    scoped_refptr<base::SequencedTaskRunner> reply_runner_;
    StreamWrittenCallback callback_;

    size_t segment_index_ = 0;
    uint64_t segment_offset_ = 0;
    uint64_t bytes_written_ = 0;
  };

  void NextElementOrDone() {
    if (current_element_ >= elements_.size()) {
      file_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &FileTransportStrategy::FinalizeFilesOnFileSequence,
              std::move(files_), reply_runner_,
              base::BindOnce(&FileTransportStrategy::OnFilesFinalized,
                             weak_factory_.GetWeakPtr())));
      return;
    }

    Element& element = elements_[current_element_];
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = static_cast<uint32_t>(
        std::min<uint64_t>(element.length, limits_.max_shared_memory_size));
    if (CreateDataPipe(&options, producer_handle, consumer_handle) !=
        MOJO_RESULT_OK) {
      DVLOG(1) << "Unable to create data pipe for blob transfer.";
      std::move(result_callback_).Run(BlobStatus::ERR_OUT_OF_MEMORY);
      return;
    }

    element.provider->RequestAsStream(std::move(producer_handle));
    file_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StreamToFilesWriter::CreateAndStart, std::move(files_),
                       std::move(consumer_handle), element.segments,
                       element.length, reply_runner_,
                       base::BindOnce(&FileTransportStrategy::OnElementWritten,
                                      weak_factory_.GetWeakPtr())));
  }

  void OnElementWritten(std::vector<base::File> files, uint64_t bytes_written) {
    files_ = std::move(files);
    if (bytes_written < elements_[current_element_].length) {
      std::move(result_callback_).Run(BlobStatus::ERR_FILE_WRITE_FAILED);
      return;
    }
    current_element_++;
    NextElementOrDone();
  }

  static void FinalizeFilesOnFileSequence(
      std::vector<base::File> files,
      scoped_refptr<base::SequencedTaskRunner> reply_runner,
      base::OnceCallback<void(std::vector<base::Time>)> callback) {
    std::vector<base::Time> mtimes;
    mtimes.reserve(files.size());
    for (auto& file : files) {
      base::File::Info info;
      if (!file.IsValid() || !file.Flush() || !file.GetInfo(&info)) {
        mtimes.clear();
        break;
      }
      mtimes.push_back(info.last_modified);
    }
    reply_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(mtimes)));
  }

  void OnFilesFinalized(std::vector<base::Time> mtimes) {
    if (mtimes.size() != file_references_.size()) {
      std::move(result_callback_).Run(BlobStatus::ERR_FILE_WRITE_FAILED);
      return;
    }
    for (Element& element : elements_) {
      DCHECK_EQ(element.segments.size(), element.future_files.size());
      for (size_t i = 0; i < element.segments.size(); ++i) {
        size_t file_index = element.segments[i].file_index;
        bool populate_result = element.future_files[i].Populate(
            file_references_[file_index], mtimes[file_index]);
        DCHECK(populate_result);
      }
    }
    std::move(result_callback_).Run(BlobStatus::DONE);
  }

  const BlobStorageLimits limits_;
  scoped_refptr<base::SequencedTaskRunner> reply_runner_;
  scoped_refptr<base::SequencedTaskRunner> file_runner_;

  // State used to assign bytes elements to individual files.
  size_t file_count_ = 0;
  // How big the current file already is.
  uint64_t current_file_size_ = 0;

  std::vector<Element> elements_;
  size_t current_element_ = 0;

  // Populated in BeginTransport. |files_| is moved to |file_runner_| while a
  // write is in progress and moved back when it completes.
  std::vector<base::File> files_;
  std::vector<scoped_refptr<ShareableFileReference>> file_references_;

  base::WeakPtrFactory<FileTransportStrategy> weak_factory_{this};
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
}

BlobTransportStrategy::~BlobTransportStrategy() = default;

BlobTransportStrategy::BlobTransportStrategy(BlobDataBuilder* builder,
                                             ResultCallback result_callback)
    : builder_(builder), result_callback_(std::move(result_callback)) {}

}  // namespace storage
