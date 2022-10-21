// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/file_handler_info.h"

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "components/version_info/channel.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
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
  RunTestcases(testcases, std::size(testcases), EXPECT_TYPE_ERROR);
}

TEST_F(FileHandlersManifestTest, ValidFileHandlers) {
  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess("file_handlers_valid.json");

  ASSERT_TRUE(extension.get());
  const FileHandlersInfo* handlers =
      FileHandlers::GetFileHandlers(extension.get());
  ASSERT_TRUE(handlers != nullptr);
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
  ASSERT_TRUE(handlers == nullptr);
}

class FileHandlersManifestV3Test : public ManifestTest {
 public:
  FileHandlersManifestV3Test() : channel_(version_info::Channel::CANARY) {
    feature_list_.InitAndEnableFeature(extensions_features::kFileHandlersMV3);
  }

 protected:
  ManifestData GetManifestData(const char* manifest_part) {
    static constexpr char kManifestStub[] =
        R"({
        "name": "Test",
        "version": "0.0.1",
        "manifest_version": 3,
        "file_handlers": %s
      })";
    base::Value manifest_value =
        base::test::ParseJson(base::StringPrintf(kManifestStub, manifest_part));
    EXPECT_EQ(base::Value::Type::DICTIONARY, manifest_value.type());
    return ManifestData(std::move(manifest_value));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  extensions::ScopedCurrentChannel channel_{version_info::Channel::CANARY};
};

TEST_F(FileHandlersManifestV3Test, MV3) {
  // Minimum required entry.
  {
    scoped_refptr<Extension> extension(
        LoadAndExpectSuccess(std::move(GetManifestData(R"(
        [{
          "name": "Comma separated values",
          "action": "/open-csv"
        }])"))));
    ASSERT_TRUE(FileHandlers::GetFileHandlersMV3(extension.get()));
  }

  // Error when name is missing.
  LoadAndExpectError(GetManifestData(R"(
      [{
        "action": "/open-csv"
      }])"),
                     "Error at key 'file_handlers'. Parsing array failed at "
                     "index 0: 'name' is required");

  // Error when action is missing.
  LoadAndExpectError(GetManifestData(R"(
      [{
        "name": "Comma separated values"
      }])"),
                     "Error at key 'file_handlers'. Parsing array failed at "
                     "index 0: 'action' is required");

  // Succeed with usage of all possible available keys.
  LoadAndExpectSuccess(GetManifestData(R"([{
    "name": "Comma separated values",
    "action": "/csv",
    "icons": [{
      "src": "/csv.png",
      "sizes": "16x16",
      "type": "image/png"
    }],
    "launch-type": "single-client"
  }])"));

  // Succeed with a different launch type and multiple icon sizes.
  LoadAndExpectSuccess(GetManifestData(R"([{
    "name": "Comma separated values",
    "action": "/csv",
    "icons": [{
      "src": "/csv.png",
      "sizes": "32x32 64x64",
      "type": "image/png"
    }],
    "launch_type": "multiple-clients"
  }])"));

  // Error with an empty array.
  LoadAndExpectError(GetManifestData(R"([])"),
                     "Invalid value for 'file_handlers[0]'. At least one File "
                     "Handler must be present.");

  // Error with an empty file handler.
  LoadAndExpectError(GetManifestData(R"([{}])"),
                     "Error at key 'file_handlers'. Parsing array failed at "
                     "index 0: 'action' is required");

  // Error with multiple missing file handlers.
  LoadAndExpectError(GetManifestData(R"([{},{}])"),
                     "Error at key 'file_handlers'. Parsing array failed at "
                     "index 0: 'action' is required");

  // Error if `name` is empty.
  LoadAndExpectError(
      GetManifestData(R"([
    {"name":"", "action":""}])"),
      "Invalid value for 'file_handlers[0]'. `name` must have a value.");

  // Error if `action` is empty.
  LoadAndExpectError(
      GetManifestData(R"([
    {"name":"test", "action":""}])"),
      "Invalid value for 'file_handlers[0]'. `action` must have a value.");

  // Succeed if `action` has a leading forward slash.
  LoadAndExpectSuccess(GetManifestData(R"([
    {"name":"test", "action":"/path"}])"));

  // Error if `action` doesn't have a leading forward slash.
  LoadAndExpectError(GetManifestData(R"([
    {"name":"test", "action":"path"}])"),
                     "Invalid value for 'file_handlers[0]'. `action` must "
                     "start with a forward slash.");

  // TODO(1313786): Verify `icons.sizes` is `"XxY"` or space delimited.
  // TODO(1313786): Mime type verification.
  // TODO(1313786): Verify values have expected pattern.
}

}  // namespace extensions
