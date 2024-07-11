// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CRASH_CRASH_FILE_UPLOADER_H_
#define REMOTING_HOST_CRASH_CRASH_FILE_UPLOADER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

// This class uploads dump files to the crash collector service. It should be
// created, called, and destroyed on the same thread. All work is posted to a
// separate IO thread which may block.
class CrashFileUploader {
 public:
  CrashFileUploader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<base::SingleThreadTaskRunner> core_task_runner);
  CrashFileUploader(const CrashFileUploader&) = delete;
  CrashFileUploader& operator=(const CrashFileUploader&) = delete;
  ~CrashFileUploader();

  void Upload(const base::FilePath& crash_guid);

 private:
  class Core;

  std::unique_ptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> core_task_runner_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CRASH_CRASH_FILE_UPLOADER_H_
