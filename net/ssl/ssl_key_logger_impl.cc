// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_key_logger_impl.h"

#include <stdio.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/thread_annotations.h"

namespace net {

namespace {
// Bound the number of outstanding writes to bound memory usage. Some
// antiviruses point this at a pipe and then read too slowly. See
// https://crbug.com/566951 and https://crbug.com/914880.
static constexpr size_t kMaxOutstandingLines = 512;
}  // namespace

// An object which performs the blocking file operations on a background
// SequencedTaskRunner.
class SSLKeyLoggerImpl::Core
    : public base::RefCountedThreadSafe<SSLKeyLoggerImpl::Core> {
 public:
  Core() {
    DETACH_FROM_SEQUENCE(sequence_checker_);
    // That the user explicitly asked for debugging information would suggest
    // waiting to flush these to disk, but some buggy antiviruses point this at
    // a pipe and hang, so we avoid blocking shutdown. If writing to a real
    // file, writes should complete quickly enough that this does not matter.
    task_runner_ = base::CreateSequencedTaskRunner(
        {base::ThreadPool(), base::MayBlock(),
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  }

  void SetFile(base::File file) {
    file_.reset(base::FileToFILE(std::move(file), "a"));
    if (!file_)
      DVLOG(1) << "Could not adopt file";
  }

  void OpenFile(const base::FilePath& path) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&Core::OpenFileImpl, this, path));
  }

  void WriteLine(const std::string& line) {
    bool was_empty;
    {
      base::AutoLock lock(lock_);
      was_empty = buffer_.empty();
      if (buffer_.size() < kMaxOutstandingLines) {
        buffer_.push_back(line);
      } else {
        lines_dropped_ = true;
      }
    }
    if (was_empty) {
      task_runner_->PostTask(FROM_HERE, base::BindOnce(&Core::Flush, this));
    }
  }

 private:
  friend class base::RefCountedThreadSafe<Core>;
  ~Core() = default;

  void OpenFileImpl(const base::FilePath& path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!file_);
    file_.reset(base::OpenFile(path, "a"));
    if (!file_)
      DVLOG(1) << "Could not open " << path.value();
  }

  void Flush() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    bool lines_dropped = false;
    std::vector<std::string> buffer;
    {
      base::AutoLock lock(lock_);
      std::swap(lines_dropped, lines_dropped_);
      std::swap(buffer, buffer_);
    }

    if (file_) {
      for (const auto& line : buffer) {
        fprintf(file_.get(), "%s\n", line.c_str());
      }
      if (lines_dropped) {
        fprintf(file_.get(), "# Some lines were dropped due to slow writes.\n");
      }
      fflush(file_.get());
    }
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::ScopedFILE file_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::Lock lock_;
  bool lines_dropped_ GUARDED_BY(lock_) = false;
  std::vector<std::string> buffer_ GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(Core);
};

SSLKeyLoggerImpl::SSLKeyLoggerImpl(const base::FilePath& path)
    : core_(base::MakeRefCounted<Core>()) {
  core_->OpenFile(path);
}

SSLKeyLoggerImpl::SSLKeyLoggerImpl(base::File file)
    : core_(base::MakeRefCounted<Core>()) {
  core_->SetFile(std::move(file));
}

SSLKeyLoggerImpl::~SSLKeyLoggerImpl() = default;

void SSLKeyLoggerImpl::WriteLine(const std::string& line) {
  core_->WriteLine(line);
}

}  // namespace net
