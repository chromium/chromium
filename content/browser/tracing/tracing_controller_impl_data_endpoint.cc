// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/pattern.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"
#include "third_party/zlib/zlib.h"

namespace content {

namespace {

class StringTraceDataEndpoint : public TracingController::TraceDataEndpoint {
 public:
  explicit StringTraceDataEndpoint(
      TracingController::CompletionCallback callback)
      : completion_callback_(std::move(callback)) {}

  StringTraceDataEndpoint(const StringTraceDataEndpoint&) = delete;
  StringTraceDataEndpoint& operator=(const StringTraceDataEndpoint&) = delete;

  void ReceivedTraceFinalContents() override {
    auto str = std::make_unique<std::string>(trace_.str());
    trace_.str("");
    trace_.clear();

    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(completion_callback_), std::move(str)));
  }

  void ReceiveTraceChunk(std::unique_ptr<std::string> chunk) override {
    trace_ << *chunk;
  }

 private:
  ~StringTraceDataEndpoint() override {}

  TracingController::CompletionCallback completion_callback_;
  std::ostringstream trace_;
};

class FileTraceDataEndpoint : public TracingController::TraceDataEndpoint {
 public:
  explicit FileTraceDataEndpoint(const base::FilePath& trace_file_path,
                                 base::OnceClosure callback,
                                 base::TaskPriority write_priority)
      : file_path_(trace_file_path),
        completion_callback_(std::move(callback)),
        may_block_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), write_priority,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

  FileTraceDataEndpoint(const FileTraceDataEndpoint&) = delete;
  FileTraceDataEndpoint& operator=(const FileTraceDataEndpoint&) = delete;

  void ReceiveTraceChunk(std::unique_ptr<std::string> chunk) override {
    may_block_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FileTraceDataEndpoint::ReceiveTraceChunkOnBlockingThread, this,
            std::move(chunk)));
  }

  void ReceivedTraceFinalContents() override {
    may_block_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FileTraceDataEndpoint::CloseOnBlockingThread, this));
  }

 private:
  ~FileTraceDataEndpoint() override { DCHECK(file_ == nullptr); }

  void ReceiveTraceChunkOnBlockingThread(std::unique_ptr<std::string> chunk) {
    if (!OpenFileIfNeededOnBlockingThread())
      return;
    std::ignore = fwrite(chunk->c_str(), chunk->size(), 1, file_.get());
  }

  bool OpenFileIfNeededOnBlockingThread() {
    if (file_ != nullptr)
      return true;

    // The temporary trace file is produced in the same folder since paths must
    // be on the same volume.
    base::File temp_file = CreateAndOpenTemporaryFileInDir(file_path_.DirName(),
                                                           &pending_file_path_);
    if (temp_file.IsValid()) {
      // On Android, fdsan prohibits associating a new stream with a file while
      // it's still owned by base::File. So we have to close it first and then
      // reopen as FILE*.
      temp_file.Close();
      file_.reset(base::OpenFile(pending_file_path_, "w"));
    } else {
      LOG(WARNING) << "Unable to use temporary file " << pending_file_path_
                   << ": "
                   << base::File::ErrorToString(temp_file.error_details());
      pending_file_path_.clear();
      file_.reset(base::OpenFile(file_path_, "w"));
      LOG_IF(ERROR, file_ == nullptr)
          << "Failed to open " << file_path_.value();
    }
    return file_ != nullptr;
  }

  void CloseOnBlockingThread() {
    if (OpenFileIfNeededOnBlockingThread()) {
      file_ = nullptr;
    }

    if (!pending_file_path_.empty()) {
      base::File::Error error;
      if (!base::ReplaceFile(pending_file_path_, file_path_, &error)) {
        LOG(ERROR) << "Cannot replace file '" << file_path_
                   << "' : " << base::File::ErrorToString(error);
        base::DeleteFile(pending_file_path_);
        return;
      }
    }

    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&FileTraceDataEndpoint::FinalizeOnUIThread, this));
  }

  void FinalizeOnUIThread() { std::move(completion_callback_).Run(); }

  base::FilePath file_path_;
  base::FilePath pending_file_path_;
  base::OnceClosure completion_callback_;
  base::ScopedFILE file_ = nullptr;
  const scoped_refptr<base::SequencedTaskRunner> may_block_task_runner_;
};

class CompressedTraceDataEndpoint
    : public TracingController::TraceDataEndpoint {
 public:
  CompressedTraceDataEndpoint(scoped_refptr<TraceDataEndpoint> endpoint,
                              bool compress_with_background_priority)
      : endpoint_(endpoint),
        already_tried_open_(false),
        background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {compress_with_background_priority
                 ? base::TaskPriority::BEST_EFFORT
                 : base::TaskPriority::USER_VISIBLE})) {}

  CompressedTraceDataEndpoint(const CompressedTraceDataEndpoint&) = delete;
  CompressedTraceDataEndpoint& operator=(const CompressedTraceDataEndpoint&) =
      delete;

  void ReceiveTraceChunk(std::unique_ptr<std::string> chunk) override {
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CompressedTraceDataEndpoint::CompressOnBackgroundThread,
                       this, std::move(chunk)));
  }

  void ReceivedTraceFinalContents() override {
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CompressedTraceDataEndpoint::CloseOnBackgroundThread,
                       this));
  }

 private:
  ~CompressedTraceDataEndpoint() override = default;

  bool OpenZStreamOnBackgroundThread() {
    if (stream_)
      return true;

    if (already_tried_open_)
      return false;

    already_tried_open_ = true;
    stream_ = std::make_unique<z_stream>();
    *stream_ = {nullptr};
    stream_->zalloc = Z_NULL;
    stream_->zfree = Z_NULL;
    stream_->opaque = Z_NULL;

    int result = deflateInit2(stream_.get(), Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                              // 16 is added to produce a gzip header + trailer.
                              MAX_WBITS + 16,
                              8,  // memLevel = 8 is default.
                              Z_DEFAULT_STRATEGY);
    return result == 0;
  }

  void CompressOnBackgroundThread(std::unique_ptr<std::string> chunk) {
    if (!OpenZStreamOnBackgroundThread())
      return;

    stream_->avail_in = chunk->size();
    stream_->next_in = reinterpret_cast<unsigned char*>(&*chunk->begin());
    DrainStreamOnBackgroundThread(false);
  }

  void DrainStreamOnBackgroundThread(bool finished) {
    int err;
    const int kChunkSize = 0x4000;
    char buffer[kChunkSize];
    do {
      stream_->avail_out = kChunkSize;
      stream_->next_out = (unsigned char*)buffer;
      err = deflate(stream_.get(), finished ? Z_FINISH : Z_NO_FLUSH);
      if (err != Z_OK && (!finished || err != Z_STREAM_END)) {
        LOG(ERROR) << "Deflate stream error: " << err;
        stream_.reset();
        return;
      }

      int bytes = kChunkSize - stream_->avail_out;
      if (bytes) {
        std::string compressed(buffer, bytes);
        endpoint_->ReceiveTraceChunk(std::make_unique<std::string>(compressed));
      }
    } while (stream_->avail_out == 0);
  }

  void CloseOnBackgroundThread() {
    if (!OpenZStreamOnBackgroundThread())
      return;

    DrainStreamOnBackgroundThread(true);
    deflateEnd(stream_.get());
    stream_.reset();

    endpoint_->ReceivedTraceFinalContents();
  }

  scoped_refptr<TraceDataEndpoint> endpoint_;
  std::unique_ptr<z_stream> stream_;
  bool already_tried_open_;
  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
};

}  // namespace

scoped_refptr<TracingController::TraceDataEndpoint>
TracingController::CreateStringEndpoint(CompletionCallback callback) {
  return new StringTraceDataEndpoint(std::move(callback));
}

scoped_refptr<TracingController::TraceDataEndpoint>
TracingController::CreateFileEndpoint(const base::FilePath& file_path,
                                      base::OnceClosure callback,
                                      base::TaskPriority write_priority) {
  return new FileTraceDataEndpoint(file_path, std::move(callback),
                                   write_priority);
}

scoped_refptr<TracingController::TraceDataEndpoint>
TracingControllerImpl::CreateCompressedStringEndpoint(
    scoped_refptr<TraceDataEndpoint> endpoint,
    bool compress_with_background_priority) {
  return new CompressedTraceDataEndpoint(endpoint,
                                         compress_with_background_priority);
}

scoped_refptr<TracingController::TraceDataEndpoint>
TracingControllerImpl::CreateCallbackEndpoint(CompletionCallback callback) {
  return new StringTraceDataEndpoint(std::move(callback));
}

}  // namespace content
