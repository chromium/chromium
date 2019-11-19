// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/feature_list.h"
#include "content/renderer/render_process.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

base::ThreadPriority GetRenderIOThreadPriority() {
  if (base::FeatureList::IsEnabled(
          blink::features::kBlinkCompositorUseDisplayThreadPriority))
    return base::ThreadPriority::DISPLAY;
  return base::ThreadPriority::NORMAL;
}

}  // namespace

RenderProcess::RenderProcess(
    const std::string& thread_pool_name,
    std::unique_ptr<base::ThreadPoolInstance::InitParams>
        thread_pool_init_params)
    : ChildProcess(GetRenderIOThreadPriority(),
                   thread_pool_name,
                   std::move(thread_pool_init_params)) {}

}  // namespace content
