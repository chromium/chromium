// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/coreml_initializer.h"

#import <Foundation/Foundation.h>

#include <string_view>
#include <utility>

#include "base/apple/foundation_util.h"
#include "base/base_paths_posix.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/one_shot_event.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"

namespace webnn {

namespace {

void LogSetupError(std::string_view what) {
  LOG(ERROR) << "Failed to " << what
             << " for WebNN, this might affect the performance and accuracy "
                "for WebNN.";
}

void SetupCacheDir(const base::FilePath& gpu_helper_app_path) {
  // Unbundled, e.g. in tests: the GPU sandbox doesn't allow the directory.
  if (gpu_helper_app_path.empty()) {
    return;
  }

  // Must match helper-bundle-id in gpu.sb, which is read from the same bundle.
  NSString* helper_bundle_id =
      [NSBundle
          bundleWithPath:base::apple::FilePathToNSString(gpu_helper_app_path)]
          .bundleIdentifier;
  if (!helper_bundle_id) {
    LogSetupError("get GPU helper bundle ID");
    return;
  }

  base::FilePath cache_dir;
  if (!base::PathService::Get(base::DIR_CACHE, &cache_dir)) {
    LogSetupError("get cache directory");
    return;
  }
  cache_dir =
      cache_dir.Append(base::apple::NSStringToFilePath(helper_bundle_id))
          .Append("com.apple.e5rt.e5bundlecache");
  if (!base::CreateDirectory(cache_dir)) {
    LogSetupError("setup cache directory");
  }
}

class WebNNCoreMLInitializer {
 public:
  explicit WebNNCoreMLInitializer(const base::FilePath& gpu_helper_app_path) {
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        {base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
        base::BindOnce(&SetupCacheDir, gpu_helper_app_path),
        base::BindOnce(&WebNNCoreMLInitializer::OnCacheDirSetupDone,
                       base::Unretained(this)));
  }

  ~WebNNCoreMLInitializer() = delete;

  void RunAfterInitialize(base::OnceClosure callback) {
    initialized_.Post(FROM_HERE, std::move(callback));
  }

 private:
  void OnCacheDirSetupDone() { initialized_.Signal(); }

  base::OneShotEvent initialized_;
};

}  // namespace

void InitializeCacheDirAndRun(const base::FilePath& gpu_helper_app_path,
                              base::OnceClosure callback) {
  // We only need to initialize once for the whole browser process, and no
  // teardown logic is needed, so use a global singleton here.
  static base::NoDestructor<WebNNCoreMLInitializer> g_webnn_coreml_initializer(
      gpu_helper_app_path);
  g_webnn_coreml_initializer->RunAfterInitialize(std::move(callback));
}

void SetupCacheDirForTesting(const base::FilePath& gpu_helper_app_path) {
  SetupCacheDir(gpu_helper_app_path);
}

}  // namespace webnn
