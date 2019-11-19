// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/stl_util.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

typedef ManifestTest FileHandlersManifestTest;

TEST_F(FileHandlersManifestTest, InvalidFileHandlers) {
  Testcase testcases[] = {
      Testcase("file_handlers_invalid_handlers.json",
               errors::kInvalidFileHandlers),
      Testcase("file_handlers_invalid_type.json",
               errors::kInvalidFileHandlerType),
      Testcase("file_handlers_invalid_extension.json",
               errors::kInvalidFileHandlerExtension),
      Testcase("file_handlers_invalid_no_type_or_extension.json",
               errors::kInvalidFileHandlerNoTypeOrExtension),
      Testcase("file_handlers_invalid_type_element.json",
               errors::kInvalidFileHandlerTypeElement),
      Testcase("file_handlers_invalid_extension_element.json",
               errors::kInvalidFileHandlerExtensionElement),
      Testcase("file_handlers_invalid_too_many.json",
               errors::kInvalidFileHandlersTooManyTypesAndExtensions),
      Testcase("file_handlers_invalid_include_directories.json",
               errors::kInvalidFileHandlerIncludeDirectories),
      Testcase("file_handlers_invalid_verb.json",
               errors::kInvalidFileHandlerVerb),
  };
  RunTestcases(testcases, base::size(testcases), EXPECT_TYPE_ERROR);
}

TEST_F(FileHandlersManifestTest, ValidFileHandlers) {
  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess("file_handlers_valid.json");

  ASSERT_TRUE(extension.get());
  const FileHandlersInfo* handlers =
      FileHandlers::GetFileHandlers(extension.get());
  ASSERT_TRUE(handlers != NULL);
  ASSERT_EQ(3U, handlers->size());

  apps::FileHandlerInfo handler = handlers->at(0);
  EXPECT_EQ("directories", handler.id);
  EXPECT_EQ(0U, handler.types.size());
  EXPECT_EQ(1U, handler.extensions.size());
  EXPECT_EQ(1U, handler.extensions.count("*/*"));
  EXPECT_EQ(true, handler.include_directories);

  handler = handlers->at(1);
  EXPECT_EQ("image", handler.id);
  EXPECT_EQ(1U, handler.types.size());
  EXPECT_EQ(1U, handler.types.count("image/*"));
  EXPECT_EQ(2U, handler.extensions.size());
  EXPECT_EQ(1U, handler.extensions.count(".png"));
  EXPECT_EQ(1U, handler.extensions.count(".gif"));
  EXPECT_EQ("add_to", handler.verb);

  handler = handlers->at(2);
  EXPECT_EQ("text", handler.id);
  EXPECT_EQ(1U, handler.types.size());
  EXPECT_EQ(1U, handler.types.count("text/*"));
  EXPECT_EQ(0U, handler.extensions.size());
}

TEST_F(FileHandlersManifestTest, NotPlatformApp) {
  // This should load successfully but have the file handlers ignored.
  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess("file_handlers_invalid_not_app.json");

  ASSERT_TRUE(extension.get());
  const FileHandlersInfo* handlers =
      FileHandlers::GetFileHandlers(extension.get());
  ASSERT_TRUE(handlers == NULL);
}

TEST_F(FileHandlersManifestTest, HostedNotBookmarkApp) {
  // This should load successfully but have the file handlers ignored.
  scoped_refptr<const Extension> extension = LoadAndExpectSuccess(
      "file_handlers_valid_hosted_app.json", extensions::Manifest::INTERNAL);

  ASSERT_TRUE(extension);

  std::vector<InstallWarning> expected_warnings;
  expected_warnings.push_back(
      InstallWarning(errors::kInvalidFileHandlersHostedAppsNotSupported));
  EXPECT_EQ(expected_warnings, extension->install_warnings());

  EXPECT_TRUE(extension->is_hosted_app());
  EXPECT_FALSE(extension->from_bookmark());

  const FileHandlersInfo* handlers =
      FileHandlers::GetFileHandlers(extension.get());
  EXPECT_FALSE(handlers);
}

TEST_F(FileHandlersManifestTest, HostedBookmarkApp) {
  // This should load successfully with file handlers.
  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess("file_handlers_valid_hosted_app.json",
                           extensions::Manifest::Location::INTERNAL,
                           extensions::Extension::FROM_BOOKMARK);

  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->install_warnings().empty());

  // Check we're a hosted app and a bookmark app.
  EXPECT_TRUE(extension->is_hosted_app());
  EXPECT_TRUE(extension->from_bookmark());

  const FileHandlersInfo* handlers =
      FileHandlers::GetFileHandlers(extension.get());
  ASSERT_TRUE(handlers);
  EXPECT_GE(handlers->size(), 1u);
}

}  // namespace extensions
