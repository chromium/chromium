// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/control_key.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

TEST(ControlKeyTest, HashEquivalence) {
  {
    AtomicString name("name1");
    AtomicString type("type1");
    EXPECT_EQ(ControlKeyHashTraits::GetHash(ControlKey(name, type)),
              ControlKeyTranslator::GetHash({name, type}));
  }

  EXPECT_EQ(
      ControlKeyHashTraits::GetHash(ControlKey(g_empty_atom, g_empty_atom)),
      ControlKeyTranslator::GetHash({g_empty_atom, g_empty_atom}));
}

}  // namespace blink
