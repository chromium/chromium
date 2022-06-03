// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_TOP_LEVEL_BLAME_CONTEXT_H_
#define CONTENT_RENDERER_TOP_LEVEL_BLAME_CONTEXT_H_

#include "base/trace_event/blame_context.h"

namespace content {

// A blame context which spans all the frames in this renderer. Used for
// attributing work which cannot be associated with a specific frame (e.g.,
// garbage collection).
class TopLevelBlameContext : public base::trace_event::BlameContext {
 public:
  TopLevelBlameContext();

  TopLevelBlameContext(const TopLevelBlameContext&) = delete;
  TopLevelBlameContext& operator=(const TopLevelBlameContext&) = delete;
};

}  // namespace content

#endif  // CONTENT_RENDERER_TOP_LEVEL_BLAME_CONTEXT_H_
