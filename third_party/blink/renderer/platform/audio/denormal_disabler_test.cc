// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/denormal_disabler.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

bool DenormalsAreFlushedToZero() {
  volatile double denorm = 2.225e-308;
  return !((denorm / 2.0) > 0.0);
}

TEST(DenormalDisablerTest, DisableScoped) {
  const bool already_flushed = DenormalsAreFlushedToZero();
  if (!already_flushed) {
    DenormalDisabler scoped_disabler;
    EXPECT_TRUE(DenormalsAreFlushedToZero());
  }
}

TEST(DenormalDisablerTest, EnableScoped) {
  const bool already_flushed = DenormalsAreFlushedToZero();
  if (!already_flushed) {
    DenormalDisabler scoped_disabler;
    EXPECT_TRUE(DenormalsAreFlushedToZero());
    {
      DenormalEnabler scoped_enabler;
      EXPECT_FALSE(DenormalsAreFlushedToZero());
    }
    EXPECT_TRUE(DenormalsAreFlushedToZero());
  }
}

TEST(DenormalDisablerTest, ModifyUnscoped) {
  const bool already_flushed = DenormalsAreFlushedToZero();
  if (!already_flushed) {
    DenormalModifier::DisableDenormals();
    EXPECT_TRUE(DenormalsAreFlushedToZero());
    DenormalModifier::EnableDenormals();
    EXPECT_FALSE(DenormalsAreFlushedToZero());
  }
}

}  // namespace

}  // namespace blink
