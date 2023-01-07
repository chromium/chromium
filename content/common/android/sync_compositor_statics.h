// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_SYNC_COMPOSITOR_STATICS_H_
#define CONTENT_COMMON_ANDROID_SYNC_COMPOSITOR_STATICS_H_

class SkCanvas;

namespace content {

// Used to pass the SkCanvas for drawing across the
// SyncCompositorMsg_DemandDrawSw IPC, between SynchronousCompositorHost and
// SynchronousCompositorProxy. Access is synchronized by that IPC and its reply,
// so there are no internal locks.
void SynchronousCompositorSetSkCanvas(SkCanvas* canvas);
SkCanvas* SynchronousCompositorGetSkCanvas();

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_SYNC_COMPOSITOR_STATICS_H_
