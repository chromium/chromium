// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mhtml_handle_writer.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "content/common/download/mhtml_file_writer.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "third_party/blink/public/platform/web_thread_safe_data.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace content {

MHTMLHandleWriter::MHTMLHandleWriter(
    scoped_refptr<base::TaskRunner> main_thread_task_runner,
    MHTMLWriteCompleteCallback callback)
    : main_thread_task_runner_(std::move(main_thread_task_runner)),
      callback_(std::move(callback)) {}

MHTMLHandleWriter::~MHTMLHandleWriter() = default;

void MHTMLHandleWriter::WriteContents(
    std::unique_ptr<MHTMLHandleWriter> self,
    std::vector<blink::WebThreadSafeData> mhtml_contents) {
  TRACE_EVENT_BEGIN(
      "page-serialization", "Writing MHTML contents to handle",
      perfetto::NamedTrack::FromPointer("content::MHTMLHandleWriter", this));
  is_writing_ = true;
  WriteContentsImpl(std::move(self), std::move(mhtml_contents));
}

void MHTMLHandleWriter::Finish(std::unique_ptr<MHTMLHandleWriter> self,
                               mojom::MhtmlSaveStatus save_status) {
  DCHECK(!RenderThread::IsMainThread())
      << "Should not run in the main renderer thread";
  if (is_writing_) {
    TRACE_EVENT_END(
        "page-serialization",
        perfetto::NamedTrack::FromPointer("content::MHTMLHandleWriter", this));
  }
  Close();

  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), save_status));
}

MHTMLFileHandleWriter::MHTMLFileHandleWriter(
    scoped_refptr<base::TaskRunner> main_thread_task_runner,
    MHTMLWriteCompleteCallback callback,
    base::File file)
    : MHTMLHandleWriter(std::move(main_thread_task_runner),
                        std::move(callback)),
      file_(std::move(file)) {
#if BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/42050414): Remove the Seek call.
  // On fuchsia, fds do not share state. As the fd has been duped and sent from
  // the browser process, it must be seeked to the end to ensure the data is
  // appended.
  file_.Seek(base::File::FROM_END, 0);
#endif  // BUILDFLAG(IS_FUCHSIA)
}

MHTMLFileHandleWriter::~MHTMLFileHandleWriter() {}

void MHTMLFileHandleWriter::WriteContentsImpl(
    std::unique_ptr<MHTMLHandleWriter> self,
    std::vector<blink::WebThreadSafeData> mhtml_contents) {
  for (const auto& data : mhtml_contents) {
    if (data.IsEmpty()) {
      continue;
    }
    if (!file_.WriteAtCurrentPosAndCheck(base::as_byte_span(data))) {
      Finish(std::move(self), mojom::MhtmlSaveStatus::kFileWritingError);
      return;
    }
  }
  Finish(std::move(self), mojom::MhtmlSaveStatus::kSuccess);
}

void MHTMLFileHandleWriter::Close() {
  file_.Close();
}

namespace {

class MHTMLDataSource : public mojo::DataPipeProducer::DataSource {
 public:
  explicit MHTMLDataSource(std::vector<blink::WebThreadSafeData> mhtml_contents)
      : mhtml_contents_(std::move(mhtml_contents)) {
    for (const auto& data : mhtml_contents_) {
      total_size_ += data.size();
    }
  }

  MHTMLDataSource(const MHTMLDataSource&) = delete;
  MHTMLDataSource& operator=(const MHTMLDataSource&) = delete;

  ~MHTMLDataSource() override = default;

  // mojo::DataPipeProducer::DataSource:
  uint64_t GetLength() const override { return total_size_; }

  bool MayInspectReadBuffer() const override { return false; }

  ReadResult Read(uint64_t offset, base::span<char> buffer) override {
    ReadResult result;
    if (offset != current_offset_) {
      result.result = MOJO_RESULT_INVALID_ARGUMENT;
      return result;
    }

    size_t bytes_to_read = buffer.size();
    size_t bytes_read = 0;

    while (bytes_to_read > 0 &&
           current_buffer_index_ < mhtml_contents_.size()) {
      const auto& data = mhtml_contents_[current_buffer_index_];
      size_t data_size = data.size();

      DCHECK_GE(data_size, offset_in_current_buffer_);
      size_t available_in_buffer = data_size - offset_in_current_buffer_;

      if (available_in_buffer == 0) {
        current_buffer_index_++;
        offset_in_current_buffer_ = 0;
        continue;
      }

      size_t copy_size = std::min(bytes_to_read, available_in_buffer);

      base::as_writable_bytes(buffer.subspan(bytes_read, copy_size))
          .copy_from(base::as_byte_span(data).subspan(offset_in_current_buffer_,
                                                      copy_size));

      bytes_read += copy_size;
      bytes_to_read -= copy_size;
      offset_in_current_buffer_ += copy_size;
      current_offset_ += copy_size;

      if (offset_in_current_buffer_ == data_size) {
        current_buffer_index_++;
        offset_in_current_buffer_ = 0;
      }
    }

    result.bytes_read = bytes_read;
    result.result = MOJO_RESULT_OK;
    return result;
  }

 private:
  const std::vector<blink::WebThreadSafeData> mhtml_contents_;
  uint64_t total_size_ = 0;
  size_t current_buffer_index_ = 0;
  size_t offset_in_current_buffer_ = 0;
  uint64_t current_offset_ = 0;
};

}  // namespace

MHTMLProducerHandleWriter::MHTMLProducerHandleWriter(
    scoped_refptr<base::TaskRunner> main_thread_task_runner,
    MHTMLWriteCompleteCallback callback,
    mojo::ScopedDataPipeProducerHandle producer)
    : MHTMLHandleWriter(std::move(main_thread_task_runner),
                        std::move(callback)),
      producer_(std::make_unique<mojo::DataPipeProducer>(std::move(producer))) {
}

void MHTMLProducerHandleWriter::WriteContentsImpl(
    std::unique_ptr<MHTMLHandleWriter> self,
    std::vector<blink::WebThreadSafeData> mhtml_contents) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&MHTMLProducerHandleWriter::BeginWriting,
                                       base::Unretained(this), std::move(self),
                                       std::move(mhtml_contents)));
}

MHTMLProducerHandleWriter::~MHTMLProducerHandleWriter() = default;

void MHTMLProducerHandleWriter::Close() {
  producer_.reset();
}

void MHTMLProducerHandleWriter::BeginWriting(
    std::unique_ptr<MHTMLHandleWriter> self,
    std::vector<blink::WebThreadSafeData> mhtml_contents) {
  auto data_source =
      std::make_unique<MHTMLDataSource>(std::move(mhtml_contents));
  producer_->Write(std::move(data_source),
                   base::BindOnce(&MHTMLProducerHandleWriter::OnWriteComplete,
                                  base::Unretained(this), std::move(self)));
}

void MHTMLProducerHandleWriter::OnWriteComplete(
    std::unique_ptr<MHTMLHandleWriter> self,
    MojoResult result) {
  mojom::MhtmlSaveStatus status = mojom::MhtmlSaveStatus::kSuccess;
  if (result != MOJO_RESULT_OK) {
    status = mojom::MhtmlSaveStatus::kStreamingError;
  }
  Finish(std::move(self), status);
}

}  // namespace content
