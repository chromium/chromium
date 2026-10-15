// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/icons/extension_icon_set.h"

#include <set>

#include "base/files/file_path.h"
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

// Tests that the "any" icon is used as a fallback for all match types.
TEST(ExtensionIconSetTest, AnySize) {
  ExtensionIconSet icons;
  icons.SetAny("any.svg");
  EXPECT_FALSE(icons.empty());
  EXPECT_TRUE(icons.map().empty());
  EXPECT_TRUE(icons.ContainsPath("any.svg"));

  EXPECT_EQ("any.svg", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                                 ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("any.svg", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                                 ExtensionIconSet::Match::kSmaller));
  EXPECT_EQ("any.svg", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                                 ExtensionIconSet::Match::kBigger));

  icons.Add(extension_misc::EXTENSION_ICON_LARGE, "large.svg");
  // A sized icon is preferred over "any" when it satisfies the query.
  EXPECT_EQ("large.svg", icons.Get(extension_misc::EXTENSION_ICON_LARGE,
                                   ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("large.svg", icons.Get(extension_misc::EXTENSION_ICON_EXTRA_LARGE,
                                   ExtensionIconSet::Match::kSmaller));
  // No sized icon is <= medium, so kSmaller falls back to "any".
  EXPECT_EQ("any.svg", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                                 ExtensionIconSet::Match::kSmaller));

  std::set<base::FilePath> paths;
  icons.GetPaths(&paths);
  EXPECT_EQ(2u, paths.size());
  EXPECT_TRUE(paths.contains(base::FilePath::FromUTF8Unsafe("any.svg")));

  icons.Clear();
  EXPECT_TRUE(icons.empty());
  EXPECT_EQ("", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                          ExtensionIconSet::Match::kExactly));
}

// Tests that a regular 512px icon isn't treated as an "any" fallback.
TEST(ExtensionIconSetTest, GigantorIsNotAny) {
  ExtensionIconSet icons;
  icons.Add(extension_misc::EXTENSION_ICON_GIGANTOR, "512.png");

  EXPECT_EQ("", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                          ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                          ExtensionIconSet::Match::kSmaller));
  EXPECT_EQ("512.png", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                                 ExtensionIconSet::Match::kBigger));
}

}  // namespace
