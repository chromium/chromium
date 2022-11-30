// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_INIT_IOS_GLOBAL_STATE_CONFIGURATION_H_
#define IOS_WEB_PUBLIC_INIT_IOS_GLOBAL_STATE_CONFIGURATION_H_

#include "base/task/single_thread_task_runner.h"

namespace ios_global_state {

// Returns a TaskRunner for running tasks on the IO thread.
scoped_refptr<base::SingleThreadTaskRunner>
GetSharedNetworkIOThreadTaskRunner();

}  // namespace ios_global_state

#endif  // IOS_WEB_PUBLIC_INIT_IOS_GLOBAL_STATE_CONFIGURATION_H_
