// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/directory_lister.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

bool IsDotDot(const base::FilePath& path) {
  return FILE_PATH_LITERAL("..") == path.BaseName().value();
}

// Comparator for sorting lister results. This uses the locale aware filename
// comparison function on the filenames for sorting in the user's locale.
// Static.
bool CompareAlphaDirsFirst(const DirectoryLister::DirectoryListerData& a,
                           const DirectoryLister::DirectoryListerData& b) {
  // Parent directory before all else.
  if (IsDotDot(a.info.GetName()))
    return true;
  if (IsDotDot(b.info.GetName()))
    return false;

  // Directories before regular files.
  bool a_is_directory = a.info.IsDirectory();
  bool b_is_directory = b.info.IsDirectory();
  if (a_is_directory != b_is_directory)
    return a_is_directory;

  return base::i18n::LocaleAwareCompareFilenames(a.info.GetName(),
                                                 b.info.GetName());
}

void SortData(std::vector<DirectoryLister::DirectoryListerData>* data,
              DirectoryLister::ListingType listing_type) {
  // Sort the results. See the TODO below (this sort should be removed and we
  // should do it from JS).
  if (listing_type == DirectoryLister::ALPHA_DIRS_FIRST) {
    std::sort(data->begin(), data->end(), CompareAlphaDirsFirst);
  } else if (listing_type != DirectoryLister::NO_SORT &&
             listing_type != DirectoryLister::NO_SORT_RECURSIVE) {
    NOTREACHED();
  }
}

}  // namespace

DirectoryLister::DirectoryLister(const base::FilePath& dir,
                                 DirectoryListerDelegate* delegate)
    : DirectoryLister(dir, ALPHA_DIRS_FIRST, delegate) {}

DirectoryLister::DirectoryLister(const base::FilePath& dir,
                                 ListingType type,
                                 DirectoryListerDelegate* delegate)
    : delegate_(delegate) {
  core_ = new Core(dir, type, this);
  DCHECK(delegate_);
  DCHECK(!dir.value().empty());
}

DirectoryLister::~DirectoryLister() {
  Cancel();
}

void DirectoryLister::Start() {
  base::PostTask(FROM_HERE,
                 {base::ThreadPool(), base::MayBlock(),
                  base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                 base::BindOnce(&Core::Start, core_));
}

void DirectoryLister::Cancel() {
  core_->CancelOnOriginSequence();
}

DirectoryLister::Core::Core(const base::FilePath& dir,
                            ListingType type,
                            DirectoryLister* lister)
    : dir_(dir),
      type_(type),
      origin_task_runner_(base::SequencedTaskRunnerHandle::Get().get()),
      lister_(lister),
      cancelled_(0) {
  DCHECK(lister_);
}

DirectoryLister::Core::~Core() = default;

void DirectoryLister::Core::CancelOnOriginSequence() {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());

  base::subtle::NoBarrier_Store(&cancelled_, 1);
  // Core must not call into |lister_| after cancellation, as the |lister_| may
  // have been destroyed. Setting |lister_| to NULL ensures any such access will
  // cause a crash.
  lister_ = nullptr;
}

void DirectoryLister::Core::Start() {
  std::unique_ptr<DirectoryList> directory_list(new DirectoryList());

  if (!base::DirectoryExists(dir_)) {
    origin_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Core::DoneOnOriginSequence, this,
                       std::move(directory_list), ERR_FILE_NOT_FOUND));
    return;
  }

  int types = base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES;
  bool recursive;
  if (NO_SORT_RECURSIVE != type_) {
    types |= base::FileEnumerator::INCLUDE_DOT_DOT;
    recursive = false;
  } else {
    recursive = true;
  }
  base::FileEnumerator file_enum(dir_, recursive, types);

  base::FilePath path;
  while (!(path = file_enum.Next()).empty()) {
    // Abort on cancellation. This is purely for performance reasons.
    // Correctness guarantees are made by checks in DoneOnOriginSequence.
    if (IsCancelled())
      return;

    DirectoryListerData data;
    data.info = file_enum.GetInfo();
    data.path = path;
    data.absolute_path = base::MakeAbsoluteFilePath(path);
    directory_list->push_back(data);

    /* TODO(brettw) bug 24107: It would be nice to send incremental updates.
       We gather them all so they can be sorted, but eventually the sorting
       should be done from JS to give more flexibility in the page. When we do
       that, we can uncomment this to send incremental updates to the page.

    const int kFilesPerEvent = 8;
    if (file_data.size() < kFilesPerEvent)
      continue;

    origin_loop_->PostTask(
        FROM_HERE,
        base::BindOnce(&DirectoryLister::Core::SendData, file_data));
    file_data.clear();
    */
  }

  SortData(directory_list.get(), type_);

  origin_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::DoneOnOriginSequence, this,
                                std::move(directory_list), OK));
}

bool DirectoryLister::Core::IsCancelled() const {
  return !!base::subtle::NoBarrier_Load(&cancelled_);
}

void DirectoryLister::Core::DoneOnOriginSequence(
    std::unique_ptr<DirectoryList> directory_list,
    int error) const {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());

  // Need to check if the operation was before first callback.
  if (IsCancelled())
    return;

  for (const auto& lister_data : *directory_list) {
    lister_->OnListFile(lister_data);
    // Need to check if the operation was cancelled during the callback.
    if (IsCancelled())
      return;
  }
  lister_->OnListDone(error);
}

void DirectoryLister::OnListFile(const DirectoryListerData& data) {
  delegate_->OnListFile(data);
}

void DirectoryLister::OnListDone(int error) {
  delegate_->OnListDone(error);
}

}  // namespace net
