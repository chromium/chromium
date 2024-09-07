// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/renderer/mhtml_handle_writer.h"

#include "base/containers/span.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "content/common/download/mhtml_file_writer.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/web_thread_safe_data.h"

namespace content {

MHTMLHandleWriter::MHTMLHandleWriter(
    scoped_refptr<base::TaskRunner> main_thread_task_runner,
    MHTMLWriteCompleteCallback callback)
    : main_thread_task_runner_(std::move(main_thread_task_runner)),
      callback_(std::move(callback)) {}

MHTMLHandleWriter::~MHTMLHandleWriter() {}

void MHTMLHandleWriter::WriteContents(
    std::vector<blink::WebThreadSafeData> mhtml_contents) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("page-serialization",
                                    "Writing MHTML contents to handle",
                                    TRACE_ID_LOCAL(this));
  is_writing_ = true;
  WriteContentsImpl(std::move(mhtml_contents));
}

void MHTMLHandleWriter::Finish(mojom::MhtmlSaveStatus save_status) {
  DCHECK(!RenderThread::IsMainThread())
      << "Should not run in the main renderer thread";
  if (is_writing_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("page-serialization",
                                    "WriteContentsImpl (MHTMLHandleWriter)",
                                    TRACE_ID_LOCAL(this));
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
    std::vector<blink::WebThreadSafeData> mhtml_contents) {
  for (const auto& data : mhtml_contents) {
    if (data.IsEmpty()) {
      continue;
    }
    if (!file_.WriteAtCurrentPosAndCheck(base::as_byte_span(data))) {
      Finish(mojom::MhtmlSaveStatus::kFileWritingError);
      return;
    }
  }
  Finish(mojom::MhtmlSaveStatus::kSuccess);
}

void MHTMLFileHandleWriter::Close() {
  file_.Close();
}

MHTMLProducerHandleWriter::MHTMLProducerHandleWriter(
    scoped_refptr<base::TaskRunner> main_thread_task_runner,
    MHTMLWriteCompleteCallback callback,
    mojo::ScopedDataPipeProducerHandle producer)
    : MHTMLHandleWriter(std::move(main_thread_task_runner),
                        std::move(callback)),
      producer_(std::move(producer)),
      current_block_(0),
      write_position_(0) {}

void MHTMLProducerHandleWriter::WriteContentsImpl(
    std::vector<blink::WebThreadSafeData> mhtml_contents) {
  DCHECK(mhtml_contents_.empty());
  mhtml_contents_ = std::move(mhtml_contents);

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&MHTMLProducerHandleWriter::BeginWatchingHandle,
                                base::Unretained(this)));
}

MHTMLProducerHandleWriter::~MHTMLProducerHandleWriter() {}

void MHTMLProducerHandleWriter::Close() {
  producer_.reset();
}

void MHTMLProducerHandleWriter::BeginWatchingHandle() {
  // mojo::SimpleWatcher's constructor by default gets a reference ptr
  // to the current SequencedTaskRunner if one is not specified, keeping
  // the current SequencedTaskRunner's lifetime bound to |watcher_|.
  watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
  // Using base::Unretained is safe, as |this| owns |watcher_|.
  watcher_->Watch(
      producer_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&MHTMLProducerHandleWriter::TryWritingContents,
                          base::Unretained(this)));
}

// TODO(crbug.com/40606905): This can be simplified with usage
// of BlockingCopyToString once error signalling is implemented and
// updated with usage of base::span instead of std::string.
void MHTMLProducerHandleWriter::TryWritingContents(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    DLOG(ERROR)
        << "Error receiving notifications from producer handle watcher.";
    Finish(mojom::MhtmlSaveStatus::kStreamingError);
    return;
  }

  while (true) {
    const blink::WebThreadSafeData& data = mhtml_contents_.at(current_block_);
    base::span<const uint8_t> bytes =
        base::as_byte_span(data).subspan(write_position_);

    // If there is no more data in this block, continue to next block or
    // finish.
    if (bytes.empty()) {
      write_position_ = 0;
      if (++current_block_ >= mhtml_contents_.size()) {
        Finish(mojom::MhtmlSaveStatus::kSuccess);
        return;
      }
      continue;
    }

    size_t bytes_written = 0;
    result =
        producer_->WriteData(bytes, MOJO_WRITE_DATA_FLAG_NONE, bytes_written);

    // Break out of loop early if write was not successful to avoid
    // incrementing the write position incorrectly.
    if (result != MOJO_RESULT_OK)
      break;

    // Reaching this indicates a successful write.
    write_position_ += bytes_written;
    DCHECK(write_position_ <= data.size());
  }

  if (result != MOJO_RESULT_SHOULD_WAIT) {
    Finish(mojom::MhtmlSaveStatus::kStreamingError);
  }

  // Buffer is full, return to automatically re-arm the watcher.
}

}  // namespace content
