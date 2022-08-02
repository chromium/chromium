// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/top_level_blame_context.h"

#include "base/threading/platform_thread.h"

namespace content {

const char kTopLevelBlameContextCategory[] = "blink";
const char kTopLevelBlameContextName[] = "FrameBlameContext";
const char kTopLevelBlameContextType[] = "TopLevel";
const char kTopLevelBlameContextScope[] = "PlatformThread";

TopLevelBlameContext::TopLevelBlameContext()
    : base::trace_event::BlameContext(kTopLevelBlameContextCategory,
                                      kTopLevelBlameContextName,
                                      kTopLevelBlameContextType,
                                      kTopLevelBlameContextScope,
                                      base::PlatformThread::CurrentId(),
                                      nullptr) {}

}  // namespace content
