// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

TEST(MemoryManagedPaintRecorderTest,
     ReleaseMainRecordingReleasesAllReleasableOps) {
  MemoryManagedPaintRecorder recorder(gfx::Size(10, 10), nullptr);

  EXPECT_FALSE(recorder.HasRecordedDrawOps());
  EXPECT_FALSE(recorder.HasReleasableDrawOps());

  recorder.getRecordingCanvas().drawRect({0, 0, 10, 10}, cc::PaintFlags());
  EXPECT_TRUE(recorder.HasRecordedDrawOps());
  EXPECT_TRUE(recorder.HasReleasableDrawOps());

  // `ReleaseMainRecording` releases all ops, leaving the canvas clean.
  recorder.ReleaseMainRecording();
  EXPECT_FALSE(recorder.HasRecordedDrawOps());
  EXPECT_FALSE(recorder.HasReleasableDrawOps());
}

TEST(MemoryManagedPaintRecorderTest,
     ReleaseMainRecordingReleasesAllOpsOutsideLayers) {
  MemoryManagedPaintRecorder recorder(gfx::Size(10, 10), nullptr);

  EXPECT_FALSE(recorder.HasRecordedDrawOps());
  EXPECT_FALSE(recorder.HasReleasableDrawOps());
  EXPECT_FALSE(recorder.HasSideRecording());

  // Side canvases (used for canvas 2d layers) cannot be flushed until closed.
  // Open one and validate that releasing the main canvas only flushes that main
  // recording, not the side one.
  recorder.getRecordingCanvas().drawRect({0, 0, 10, 10}, cc::PaintFlags());
  recorder.BeginSideRecording();
  recorder.getRecordingCanvas().saveLayerAlphaf(0.5f);
  recorder.getRecordingCanvas().drawRect({0, 0, 10, 10}, cc::PaintFlags());
  EXPECT_TRUE(recorder.HasRecordedDrawOps());
  EXPECT_TRUE(recorder.HasReleasableDrawOps());
  EXPECT_TRUE(recorder.HasSideRecording());

  recorder.ReleaseMainRecording();
  EXPECT_TRUE(recorder.HasRecordedDrawOps());
  EXPECT_FALSE(recorder.HasReleasableDrawOps());
  EXPECT_TRUE(recorder.HasSideRecording());

  recorder.getRecordingCanvas().restore();
  EXPECT_TRUE(recorder.HasRecordedDrawOps());
  EXPECT_FALSE(recorder.HasReleasableDrawOps());
  EXPECT_TRUE(recorder.HasSideRecording());

  recorder.EndSideRecording();
  EXPECT_TRUE(recorder.HasRecordedDrawOps());
  EXPECT_TRUE(recorder.HasReleasableDrawOps());
  EXPECT_FALSE(recorder.HasSideRecording());

  recorder.ReleaseMainRecording();
  EXPECT_FALSE(recorder.HasRecordedDrawOps());
  EXPECT_FALSE(recorder.HasReleasableDrawOps());
  EXPECT_FALSE(recorder.HasSideRecording());
}

}  // namespace blink
