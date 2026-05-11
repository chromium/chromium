// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"

#include "base/functional/callback_helpers.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/run_loop.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

namespace blink {

class PlainTextPainterTest : public FontTestBase {
 public:
  void SetUp() override {
    FontTestBase::SetUp();
    MemoryPressureListenerRegistry::Initialize();
  }

  size_t CacheMapSizeOf(const PlainTextPainter& painter) const {
    return painter.cache_map_.size();
  }
};

TEST_F(PlainTextPainterTest, MemoryPressure) {
  // We don't cache on low-end devices.
  if (MemoryPressureListenerRegistry::IsLowEndDevice()) {
    return;
  }
  base::TestMemoryConsumerRegistry test_registry;

  auto* painter =
      MakeGarbageCollected<PlainTextPainter>(PlainTextPainter::kCanvas);
  EXPECT_EQ(0u, CacheMapSizeOf(*painter));
  Font* font = test::CreateAhemFont(40);
  painter->SegmentAndShape(TextRun("text"), *font);
  EXPECT_EQ(1u, CacheMapSizeOf(*painter));

  // Update limit to critical (0) and release memory.
  {
    base::RunLoop run_loop;
    test_registry.NotifyUpdateMemoryLimitAsync(0, base::DoNothing());
    test_registry.NotifyReleaseMemoryAsync(run_loop.QuitClosure());
    run_loop.Run();
  }

  EXPECT_EQ(0u, CacheMapSizeOf(*painter));
}

}  // namespace blink
