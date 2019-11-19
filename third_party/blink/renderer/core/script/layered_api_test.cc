// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/layered_api.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/dummy_modulator.h"

namespace blink {

namespace layered_api {

namespace {

class LayeredAPITestModulator final : public DummyModulator {
 public:
  bool BuiltInModuleInfraEnabled() const override { return true; }
  bool BuiltInModuleEnabled(layered_api::Module) const override { return true; }
};

class LayeredAPITest : public testing::Test {
 public:
  LayeredAPITest()
      : modulator_(MakeGarbageCollected<LayeredAPITestModulator>()) {}
  const Modulator& GetModulator() const { return *modulator_; }

 private:
  Persistent<LayeredAPITestModulator> modulator_;
};

TEST_F(LayeredAPITest, ResolveFetchingURL) {
  KURL base_url("https://example.com/base/path/");

  EXPECT_EQ(ResolveFetchingURL(GetModulator(), KURL("https://example.com/")),
            KURL("https://example.com/"));

  EXPECT_EQ(ResolveFetchingURL(GetModulator(), KURL("std:blank")),
            KURL("std:blank"));

  EXPECT_EQ(ResolveFetchingURL(GetModulator(), KURL("std:none")), NullURL());

  // Fallback syntax is currently disabled and rejected.
  // https://crbug.com/864748
  EXPECT_EQ(ResolveFetchingURL(GetModulator(),
                               KURL("std:blank|https://example.com/")),
            NullURL());
}

TEST_F(LayeredAPITest, GetInternalURL) {
  EXPECT_EQ(GetInternalURL(KURL("https://example.com/")), NullURL());

  EXPECT_EQ(GetInternalURL(KURL("std:blank")),
            KURL("std-internal://blank/index.mjs"));

  EXPECT_EQ(GetInternalURL(KURL("std-internal://blank/index.mjs")),
            KURL("std-internal://blank/index.mjs"));
  EXPECT_EQ(GetInternalURL(KURL("std-internal://blank/foo/bar.mjs")),
            KURL("std-internal://blank/foo/bar.mjs"));
}

TEST_F(LayeredAPITest, InternalURLRelativeResolution) {
  EXPECT_EQ(KURL(KURL("std-internal://blank/index.mjs"), "./sub.mjs"),
            KURL("std-internal://blank/sub.mjs"));
  EXPECT_EQ(KURL(KURL("std-internal://blank/index.mjs"), "/sub.mjs"),
            KURL("std-internal://blank/sub.mjs"));
  EXPECT_EQ(KURL(KURL("std-internal://blank/index.mjs"), "./foo/bar.mjs"),
            KURL("std-internal://blank/foo/bar.mjs"));
  EXPECT_EQ(KURL(KURL("std-internal://blank/foo/bar.mjs"), "../baz.mjs"),
            KURL("std-internal://blank/baz.mjs"));
}

TEST_F(LayeredAPITest, GetSourceText) {
  EXPECT_EQ(
      GetSourceText(GetModulator(), KURL("std-internal://blank/index.mjs")),
      String(""));

  EXPECT_EQ(
      GetSourceText(GetModulator(), KURL("std-internal://blank/not-found.mjs")),
      String());
  EXPECT_EQ(
      GetSourceText(GetModulator(), KURL("std-internal://none/index.mjs")),
      String());

  EXPECT_EQ(GetSourceText(GetModulator(), KURL("https://example.com/")),
            String());
}

}  // namespace

}  // namespace layered_api

}  // namespace blink
