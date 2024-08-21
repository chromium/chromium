// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_process.h"

#include <utility>

#include "base/feature_list.h"
#include "base/threading/platform_thread.h"
#include "third_party/blink/public/common/features.h"

namespace content {

RenderProcess::RenderProcess(
    std::unique_ptr<base::ThreadPoolInstance::InitParams>
        thread_pool_init_params)
    : ChildProcess(base::ThreadType::kDisplayCritical,
                   std::move(thread_pool_init_params)) {}

}  // namespace content
