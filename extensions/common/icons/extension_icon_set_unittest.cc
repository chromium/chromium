// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/icons/extension_icon_set.h"

#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(ExtensionIconSetTest, Basic) {
  ExtensionIconSet icons;
  EXPECT_EQ("", icons.Get(extension_misc::EXTENSION_ICON_LARGE,
                          ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("", icons.Get(extension_misc::EXTENSION_ICON_LARGE,
                          ExtensionIconSet::Match::kBigger));
  EXPECT_EQ("", icons.Get(extension_misc::EXTENSION_ICON_LARGE,
                          ExtensionIconSet::Match::kSmaller));
  EXPECT_TRUE(icons.map().empty());

  icons.Add(extension_misc::EXTENSION_ICON_LARGE, "large.png");
  EXPECT_EQ("large.png", icons.Get(extension_misc::EXTENSION_ICON_LARGE,
                                   ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("large.png", icons.Get(extension_misc::EXTENSION_ICON_LARGE,
                                   ExtensionIconSet::Match::kBigger));
  EXPECT_EQ("large.png", icons.Get(extension_misc::EXTENSION_ICON_LARGE,
                                   ExtensionIconSet::Match::kSmaller));
  EXPECT_EQ("large.png", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                                   ExtensionIconSet::Match::kBigger));
  EXPECT_EQ("large.png", icons.Get(extension_misc::EXTENSION_ICON_EXTRA_LARGE,
                                   ExtensionIconSet::Match::kSmaller));
  EXPECT_EQ("large.png", icons.Get(0, ExtensionIconSet::Match::kBigger));
  EXPECT_EQ("", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                          ExtensionIconSet::Match::kSmaller));
  EXPECT_EQ("", icons.Get(extension_misc::EXTENSION_ICON_EXTRA_LARGE,
                          ExtensionIconSet::Match::kBigger));

  icons.Add(extension_misc::EXTENSION_ICON_SMALLISH, "smallish.png");
  icons.Add(extension_misc::EXTENSION_ICON_SMALL, "small.png");
  icons.Add(extension_misc::EXTENSION_ICON_EXTRA_LARGE, "extra_large.png");

  EXPECT_EQ("", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                          ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("small.png", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                                   ExtensionIconSet::Match::kSmaller));
  EXPECT_EQ("large.png", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                                   ExtensionIconSet::Match::kBigger));
  EXPECT_EQ("", icons.Get(extension_misc::EXTENSION_ICON_BITTY,
                          ExtensionIconSet::Match::kSmaller));
  EXPECT_EQ("", icons.Get(extension_misc::EXTENSION_ICON_GIGANTOR,
                          ExtensionIconSet::Match::kBigger));
}

TEST(ExtensionIconSetTest, Values) {
  ExtensionIconSet icons;
  EXPECT_FALSE(icons.ContainsPath("foo"));

  icons.Add(extension_misc::EXTENSION_ICON_BITTY, "foo");
  icons.Add(extension_misc::EXTENSION_ICON_GIGANTOR, "bar");

  EXPECT_TRUE(icons.ContainsPath("foo"));
  EXPECT_TRUE(icons.ContainsPath("bar"));
  EXPECT_FALSE(icons.ContainsPath("baz"));
  EXPECT_FALSE(icons.ContainsPath(std::string()));

  icons.Clear();
  EXPECT_FALSE(icons.ContainsPath("foo"));
}

TEST(ExtensionIconSetTest, FindSize) {
  ExtensionIconSet icons;
  EXPECT_EQ(extension_misc::EXTENSION_ICON_INVALID,
            icons.GetIconSizeFromPath("foo"));

  icons.Add(extension_misc::EXTENSION_ICON_BITTY, "foo");
  icons.Add(extension_misc::EXTENSION_ICON_GIGANTOR, "bar");

  EXPECT_EQ(extension_misc::EXTENSION_ICON_BITTY,
            icons.GetIconSizeFromPath("foo"));
  EXPECT_EQ(extension_misc::EXTENSION_ICON_GIGANTOR,
            icons.GetIconSizeFromPath("bar"));
  EXPECT_EQ(extension_misc::EXTENSION_ICON_INVALID,
            icons.GetIconSizeFromPath("baz"));
  EXPECT_EQ(extension_misc::EXTENSION_ICON_INVALID,
            icons.GetIconSizeFromPath(std::string()));

  icons.Clear();
  EXPECT_EQ(extension_misc::EXTENSION_ICON_INVALID,
            icons.GetIconSizeFromPath("foo"));
}

}  // namespace
