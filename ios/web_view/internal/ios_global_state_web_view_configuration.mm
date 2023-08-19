// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/init/ios_global_state_configuration.h"

#include <dispatch/dispatch.h>

#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web_view/internal/web_view_global_state_util.h"

namespace ios_global_state {

scoped_refptr<base::SingleThreadTaskRunner>
GetSharedNetworkIOThreadTaskRunner() {
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    ios_web_view::InitializeGlobalState();
  });
  return web::GetIOThreadTaskRunner({});
}

}  // namespace ios_global_state
