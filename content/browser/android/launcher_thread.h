// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_LAUNCHER_THREAD_H_
#define CONTENT_BROWSER_ANDROID_LAUNCHER_THREAD_H_

#include "base/android/java_handler_thread.h"

#include "base/lazy_instance.h"
#include "base/task/single_thread_task_runner.h"

namespace content {
namespace android {

// This is Android's launcher thread. This should not be used directly in
// native code, but accessed through BrowserThread(Impl) instead.
class LauncherThread {
 public:
  static scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

 private:
  friend base::LazyInstanceTraitsBase<LauncherThread>;

  LauncherThread();
  ~LauncherThread();

  base::android::JavaHandlerThread java_handler_thread_;
};

}  // namespace android
}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_LAUNCHER_THREAD_H_
