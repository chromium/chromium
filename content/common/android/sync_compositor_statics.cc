// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/sync_compositor_statics.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"

namespace content {

static SkCanvas* g_canvas = nullptr;

void SynchronousCompositorSetSkCanvas(SkCanvas* canvas) {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess));
  DCHECK_NE(!!canvas, !!g_canvas);
  g_canvas = canvas;
}

SkCanvas* SynchronousCompositorGetSkCanvas() {
  return g_canvas;
}

}  // namespace content
