// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/file_handlers/directory_util.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/file_system_url.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/file_handlers/non_native_file_system_delegate.h"
#endif

namespace extensions::app_file_handler_util {

namespace {

bool GetIsDirectoryFromFileInfo(const base::FilePath& path) {
  base::File::Info file_info;
  return GetFileInfo(path, &file_info) && file_info.is_directory;
}

}  // namespace

// The callback parameter contains the result and is required to support
// both native local directories to avoid UI thread and non native local
// path directories for the IsNonNativeLocalPathDirectory API.
void GetIsDirectoryForLocalPath(content::BrowserContext* context,
                                const base::FilePath& path,
                                base::OnceCallback<void(bool)> callback) {
#if BUILDFLAG(IS_CHROMEOS)
  NonNativeFileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetNonNativeFileSystemDelegate();
  if (delegate && delegate->IsUnderNonNativeLocalPath(context, path)) {
    delegate->IsNonNativeLocalPathDirectory(context, path, std::move(callback));
    return;
  }
#endif

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetIsDirectoryFromFileInfo, path), std::move(callback));
}

IsDirectoryCollector::IsDirectoryCollector(content::BrowserContext* context)
    : context_(context), left_(0) {}

IsDirectoryCollector::~IsDirectoryCollector() = default;

void IsDirectoryCollector::CollectForEntriesPaths(
    const std::vector<base::FilePath>& paths,
    CompletionCallback callback) {
  DCHECK(!callback.is_null());
  paths_ = paths;
  callback_ = std::move(callback);

  DCHECK(!result_.get());
  result_ = std::make_unique<std::set<base::FilePath>>();
  left_ = paths.size();

  if (!left_) {
    // Nothing to process.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(result_)));
    callback_.Reset();
    return;
  }

  for (size_t i = 0; i < paths.size(); ++i) {
    GetIsDirectoryForLocalPath(
        context_, paths[i],
        base::BindOnce(&IsDirectoryCollector::OnIsDirectoryCollected,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }
}

void IsDirectoryCollector::OnIsDirectoryCollected(size_t index,
                                                  bool is_directory) {
  if (is_directory) {
    result_->insert(paths_[index]);
  }
  if (!--left_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(result_)));
    // Release the callback to avoid a circullar reference in case an instance
    // of this class is a member of a ref counted class, which instance is bound
    // to this callback.
    callback_.Reset();
  }
}

}  // namespace extensions::app_file_handler_util
