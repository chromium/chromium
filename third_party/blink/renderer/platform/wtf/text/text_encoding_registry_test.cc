// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TEST(TextEncodingRegistryTest, AllRegisteredEncodingsHaveCodecs) {
  Vector<String> aliases = TextEncodingAliasesForTesting();
  for (const String& alias : aliases) {
    TextEncoding encoding(alias);
    EXPECT_TRUE(encoding.IsValid()) << "Alias: " << alias;
    // NewTextCodec will crash if it doesn't find the codec.
    std::unique_ptr<TextCodec> codec = NewTextCodec(encoding);
    EXPECT_TRUE(codec) << "No codec for alias: " << alias
                       << " (canonical: " << encoding.GetName() << ")";
  }
}

}  // namespace blink
