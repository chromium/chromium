// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/name_traits.h"

namespace blink {

namespace {

class ClassWithoutName final {
 public:
  ClassWithoutName() = default;
};

class ClassWithName final : public NameClient {
 public:
  explicit ClassWithName(const char* name) : name_(name) {}

  const char* NameInHeapSnapshot() const final { return name_; }

 private:
  const char* name_;
};

}  // namespace

TEST(NameTraitTest, InternalNamesHiddenInOfficialBuild) {
  // Use a test instead of static_assert to allow local builds but block
  // enabling the feature accidentally through the waterfall.
  //
  // Do not include such type information in official builds to
  // (a) safe binary size on string literals, and
  // (b) avoid exposing internal types until it has been clarified whether
  //     exposing internals in DevTools is fine.
#if defined(OFFICIAL_BUILD)
  EXPECT_TRUE(NameClient::HideInternalName());
#endif
}

TEST(NameTraitTest, InternalNamesHiddenWhenFlagIsTurnedOff) {
#if !BUILDFLAG(RAW_HEAP_SNAPSHOTS)
  EXPECT_TRUE(NameClient::HideInternalName());
#endif  // BUILDFLAG(RAW_HEAP_SNAPSHOTS)
}

TEST(NameTraitTest, DefaultName) {
  ClassWithoutName no_name;
  const char* name = NameTrait<ClassWithoutName>::GetName(&no_name).value;
  if (NameClient::HideInternalName()) {
    EXPECT_EQ(0, strcmp(name, "InternalNode"));
  } else {
    EXPECT_NE(nullptr, strstr(name, "ClassWithoutName"));
  }
}

TEST(NameTraitTest, CustomName) {
  ClassWithName with_name("CustomName");
  const char* name = NameTrait<ClassWithName>::GetName(&with_name).value;
  EXPECT_EQ(0, strcmp(name, "CustomName"));
}

}  // namespace blink
