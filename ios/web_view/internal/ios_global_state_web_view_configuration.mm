// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/init/ios_global_state_configuration.h"

#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web_view/internal/web_view_global_state_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_global_state {

scoped_refptr<base::SingleThreadTaskRunner>
GetSharedNetworkIOThreadTaskRunner() {
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    ios_web_view::InitializeGlobalState();
  });
  return base::CreateSingleThreadTaskRunner({web::WebThread::IO});
}

}  // namespace ios_global_state
