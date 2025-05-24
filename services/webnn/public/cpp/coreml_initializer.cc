// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/coreml_initializer.h"

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
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"

namespace webnn {

namespace {

void SetupCacheDir() {
  base::FilePath cache_dir;
  if (!base::PathService::Get(base::DIR_CACHE, &cache_dir)) {
    LOG(ERROR) << "Failed to get cache directory for WebNN, this might "
                  "affect the performance and accuracy for WebNN.";
    return;
  }
  cache_dir =
      cache_dir.Append(base::StrCat({base::apple::BaseBundleID(), ".helper"}))
          .Append("com.apple.e5rt.e5bundlecache");
  if (!base::CreateDirectory(cache_dir)) {
    LOG(ERROR) << "Failed to setup cache directory for WebNN, this might "
                  "affect the performance and accuracy for WebNN.";
  }
}

class WebNNCoreMLInitializer {
 public:
  WebNNCoreMLInitializer() {
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        {base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
        base::BindOnce(&SetupCacheDir),
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

void InitializeCacheDirAndRun(base::OnceClosure callback) {
  // We only need to initialize once for the whole browser process, and no
  // teardown logic is needed, so use a global singleton here.
  static base::NoDestructor<WebNNCoreMLInitializer> g_webnn_coreml_initializer;
  g_webnn_coreml_initializer->RunAfterInitialize(std::move(callback));
}

}  // namespace webnn
