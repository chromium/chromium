// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/file_handlers/directory_util.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_context.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/file_system_url.h"

#if defined(OS_CHROMEOS)
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/file_handlers/non_native_file_system_delegate.h"
#endif

namespace extensions {
namespace app_file_handler_util {

namespace {

bool GetIsDirectoryFromFileInfo(const base::FilePath& path) {
  base::File::Info file_info;
  return GetFileInfo(path, &file_info) && file_info.is_directory;
}

// The callback parameter contains the result and is required to support
// both native local directories to avoid UI thread and non native local
// path directories for the IsNonNativeLocalPathDirectory API.
void EntryIsDirectory(content::BrowserContext* context,
                      const base::FilePath& path,
                      const base::Callback<void(bool)>& callback) {
#if defined(OS_CHROMEOS)
  NonNativeFileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetNonNativeFileSystemDelegate();
  if (delegate && delegate->IsUnderNonNativeLocalPath(context, path)) {
    delegate->IsNonNativeLocalPathDirectory(context, path, callback);
    return;
  }
#endif

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::Bind(&GetIsDirectoryFromFileInfo, path), callback);
}

}  // namespace

IsDirectoryCollector::IsDirectoryCollector(content::BrowserContext* context)
    : context_(context), left_(0) {}

IsDirectoryCollector::~IsDirectoryCollector() {}

void IsDirectoryCollector::CollectForEntriesPaths(
    const std::vector<base::FilePath>& paths,
    const CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  paths_ = paths;
  callback_ = callback;

  DCHECK(!result_.get());
  result_.reset(new std::set<base::FilePath>());
  left_ = paths.size();

  if (!left_) {
    // Nothing to process.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback_, std::move(result_)));
    callback_ = CompletionCallback();
    return;
  }

  for (size_t i = 0; i < paths.size(); ++i) {
    EntryIsDirectory(context_, paths[i],
                     base::Bind(&IsDirectoryCollector::OnIsDirectoryCollected,
                                weak_ptr_factory_.GetWeakPtr(), i));
  }
}

void IsDirectoryCollector::OnIsDirectoryCollected(size_t index,
                                                  bool is_directory) {
  if (is_directory)
    result_->insert(paths_[index]);
  if (!--left_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback_, std::move(result_)));
    // Release the callback to avoid a circullar reference in case an instance
    // of this class is a member of a ref counted class, which instance is bound
    // to this callback.
    callback_ = CompletionCallback();
  }
}

}  // namespace app_file_handler_util
}  // namespace extensions
