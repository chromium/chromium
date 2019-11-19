// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/test_extension_dir.h"

#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "extensions/browser/extension_creator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TestExtensionDir::TestExtensionDir() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(dir_.CreateUniqueTempDir());
  EXPECT_TRUE(crx_dir_.CreateUniqueTempDir());
}

TestExtensionDir::~TestExtensionDir() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ignore_result(dir_.Delete());
  ignore_result(crx_dir_.Delete());
}

void TestExtensionDir::WriteManifest(base::StringPiece manifest) {
  WriteFile(FILE_PATH_LITERAL("manifest.json"), manifest);
}

void TestExtensionDir::WriteManifestWithSingleQuotes(
    base::StringPiece manifest) {
  std::string double_quotes;
  base::ReplaceChars(manifest.data(), "'", "\"", &double_quotes);
  WriteManifest(double_quotes);
}

void TestExtensionDir::WriteFile(const base::FilePath::StringType& filename,
                                 base::StringPiece contents) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_EQ(base::checked_cast<int>(contents.size()),
            base::WriteFile(dir_.GetPath().Append(filename), contents.data(),
                            contents.size()));
}

base::FilePath TestExtensionDir::Pack() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ExtensionCreator creator;
  base::FilePath crx_path =
      crx_dir_.GetPath().Append(FILE_PATH_LITERAL("ext.crx"));
  base::FilePath pem_path =
      crx_dir_.GetPath().Append(FILE_PATH_LITERAL("ext.pem"));
  base::FilePath pem_in_path, pem_out_path;
  if (base::PathExists(pem_path))
    pem_in_path = pem_path;
  else
    pem_out_path = pem_path;
  if (!creator.Run(dir_.GetPath(), crx_path, pem_in_path, pem_out_path,
                   ExtensionCreator::kOverwriteCRX)) {
    ADD_FAILURE() << "ExtensionCreator::Run() failed: "
                  << creator.error_message();
    return base::FilePath();
  }
  if (!base::PathExists(crx_path)) {
    ADD_FAILURE() << crx_path.value() << " was not created.";
    return base::FilePath();
  }
  return crx_path;
}

base::FilePath TestExtensionDir::UnpackedPath() const {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // We make this absolute because it's possible that dir_ contains a symlink as
  // part of it's path. When UnpackedInstaller::GetAbsolutePath() runs as part
  // of loading the extension, the extension's path is converted to an absolute
  // path, which actually does something like `realpath` as part of its
  // resolution. If the tests are comparing paths to UnpackedPath(), then
  // they'll need to compare the same absolute'd path.
  return base::MakeAbsoluteFilePath(dir_.GetPath());
}

}  // namespace extensions
