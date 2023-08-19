// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FILE_HANDLERS_DIRECTORY_UTIL_H_
#define EXTENSIONS_BROWSER_API_FILE_HANDLERS_DIRECTORY_UTIL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace content {
class BrowserContext;
}

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
namespace app_file_handler_util {

// Gets is-a-directory-ness for a local path and returns it with |callback|.
void GetIsDirectoryForLocalPath(content::BrowserContext* context,
                                const base::FilePath& local_path,
                                base::OnceCallback<void(bool)> callback);

class IsDirectoryCollector {
 public:
  using CompletionCallback =
      base::OnceCallback<void(std::unique_ptr<std::set<base::FilePath>>)>;

  explicit IsDirectoryCollector(content::BrowserContext* context);

  IsDirectoryCollector(const IsDirectoryCollector&) = delete;
  IsDirectoryCollector& operator=(const IsDirectoryCollector&) = delete;

  virtual ~IsDirectoryCollector();

  // For the given paths obtains a set with which of them are directories.
  // The collector does not support virtual files if OS != CHROMEOS.
  void CollectForEntriesPaths(const std::vector<base::FilePath>& paths,
                              CompletionCallback callback);

 private:
  void OnIsDirectoryCollected(size_t index, bool directory);

  raw_ptr<content::BrowserContext, FlakyDanglingUntriaged> context_;
  std::vector<base::FilePath> paths_;
  std::unique_ptr<std::set<base::FilePath>> result_;
  size_t left_;
  CompletionCallback callback_;
  base::WeakPtrFactory<IsDirectoryCollector> weak_ptr_factory_{this};
};

}  // namespace app_file_handler_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FILE_HANDLERS_DIRECTORY_UTIL_H_
