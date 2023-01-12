// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/scoped_file.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"

namespace storage {

ScopedFile::ScopedFile()
    : scope_out_policy_(DONT_DELETE_ON_SCOPE_OUT) {
}

ScopedFile::ScopedFile(const base::FilePath& path,
                       ScopeOutPolicy policy,
                       scoped_refptr<base::TaskRunner> file_task_runner)
    : path_(path),
      scope_out_policy_(policy),
      file_task_runner_(std::move(file_task_runner)) {
  DCHECK(path.empty() || policy != DELETE_ON_SCOPE_OUT ||
         file_task_runner_.get())
      << "path:" << path.value() << " policy:" << policy
      << " runner:" << file_task_runner_.get();
}

ScopedFile::ScopedFile(ScopedFile&& other) {
  MoveFrom(other);
}

ScopedFile::~ScopedFile() {
  Reset();
}

void ScopedFile::AddScopeOutCallback(ScopeOutCallback callback,
                                     base::TaskRunner* callback_runner) {
  if (!callback_runner)
    callback_runner = base::SingleThreadTaskRunner::GetCurrentDefault().get();
  scope_out_callbacks_.emplace_back(std::move(callback), callback_runner);
}

base::FilePath ScopedFile::Release() {
  base::FilePath path = path_;
  path_.clear();
  scope_out_callbacks_.clear();
  scope_out_policy_ = DONT_DELETE_ON_SCOPE_OUT;
  return path;
}

void ScopedFile::Reset() {
  if (path_.empty())
    return;

  for (auto iter = scope_out_callbacks_.rbegin();
       iter != scope_out_callbacks_.rend(); ++iter) {
    iter->second->PostTask(FROM_HERE,
                           base::BindOnce(std::move(iter->first), path_));
  }

  DVLOG(1) << "ScopedFile::Reset(): "
           << (scope_out_policy_ == DELETE_ON_SCOPE_OUT ? "Deleting "
                                                        : "Not deleting ")
           << path_.value();

  if (scope_out_policy_ == DELETE_ON_SCOPE_OUT) {
    file_task_runner_->PostTask(FROM_HERE, base::GetDeleteFileCallback(path_));
  }

  // Clear all fields.
  Release();
}

void ScopedFile::MoveFrom(ScopedFile& other) {
  Reset();

  scope_out_policy_ = other.scope_out_policy_;
  scope_out_callbacks_.swap(other.scope_out_callbacks_);
  file_task_runner_ = other.file_task_runner_;
  path_ = other.Release();
}

}  // namespace storage
