// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FILE_HANDLERS_DIRECTORY_UTIL_H_
#define EXTENSIONS_BROWSER_API_FILE_HANDLERS_DIRECTORY_UTIL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace content {
class BrowserContext;
}

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
namespace app_file_handler_util {

class IsDirectoryCollector {
 public:
  using CompletionCallback =
      base::OnceCallback<void(std::unique_ptr<std::set<base::FilePath>>)>;

  explicit IsDirectoryCollector(content::BrowserContext* context);
  virtual ~IsDirectoryCollector();

  // For the given paths obtains a set with which of them are directories.
  // The collector does not support virtual files if OS != CHROMEOS.
  void CollectForEntriesPaths(const std::vector<base::FilePath>& paths,
                              CompletionCallback callback);

 private:
  void OnIsDirectoryCollected(size_t index, bool directory);

  content::BrowserContext* context_;
  std::vector<base::FilePath> paths_;
  std::unique_ptr<std::set<base::FilePath>> result_;
  size_t left_;
  CompletionCallback callback_;
  base::WeakPtrFactory<IsDirectoryCollector> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IsDirectoryCollector);
};

}  // namespace app_file_handler_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FILE_HANDLERS_DIRECTORY_UTIL_H_
