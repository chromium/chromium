// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_resource_path_normalizer.h"

#include <set>

#include "testing/gtest/include/gtest/gtest.h"

TEST(NormalizeExtensionResourcePath, NormalPath) {
  {
    base::FilePath original_path(FILE_PATH_LITERAL("icon.png"));
    base::FilePath expected_path(FILE_PATH_LITERAL("icon.png"));
    base::FilePath normal_path;
    ASSERT_TRUE(NormalizeExtensionResourcePath(original_path, &normal_path));
    EXPECT_EQ(expected_path, normal_path);
  }

  {
    base::FilePath original_path(FILE_PATH_LITERAL("src/icon.png"));
    base::FilePath expected_path =
        base::FilePath(FILE_PATH_LITERAL("src")).AppendASCII("icon.png");
    base::FilePath normal_path;
    ASSERT_TRUE(NormalizeExtensionResourcePath(original_path, &normal_path));
    EXPECT_EQ(expected_path, normal_path);
  }
}

TEST(NormalizeExtensionResourcePath, ReferencesParent) {
  for (const auto* path_str :
       {FILE_PATH_LITERAL("../icon.png"), FILE_PATH_LITERAL("src/../icon.png"),
        FILE_PATH_LITERAL("src/icons/../icon.png")}) {
    base::FilePath path(path_str);
    base::FilePath normal_path;
    EXPECT_FALSE(NormalizeExtensionResourcePath(path, &normal_path));
    EXPECT_TRUE(normal_path.empty());
  }
}

TEST(NormalizeExtensionResourcePath, EmptyPath) {
  for (const auto* path_str : {FILE_PATH_LITERAL(""), FILE_PATH_LITERAL("."),
                               FILE_PATH_LITERAL("././")}) {
    base::FilePath path(path_str);
    base::FilePath normal_path;
    EXPECT_FALSE(NormalizeExtensionResourcePath(path, &normal_path));
    EXPECT_TRUE(normal_path.empty());
  }
}

TEST(NormalizeExtensionResourcePaths, Normalization) {
  const std::set<base::FilePath> original_paths{
      base::FilePath(FILE_PATH_LITERAL("icon.png")),
      base::FilePath(FILE_PATH_LITERAL("src/icon.png")),
      base::FilePath(FILE_PATH_LITERAL("./src/././././icon.png")),
      base::FilePath(FILE_PATH_LITERAL("../")),
      base::FilePath(FILE_PATH_LITERAL("./")),
  };
  const std::set<base::FilePath> expected_paths{
      base::FilePath(FILE_PATH_LITERAL("icon.png")),
      base::FilePath(FILE_PATH_LITERAL("src")).AppendASCII("icon.png"),
  };

  const auto actual_paths = NormalizeExtensionResourcePaths(original_paths);

  EXPECT_EQ(expected_paths, actual_paths);
}
