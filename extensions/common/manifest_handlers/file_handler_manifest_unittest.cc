// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/file_handler_info.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "components/version_info/channel.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

using FileHandlersManifestTest = ManifestTest;

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

class WebFileHandlersTest : public ManifestTest {
 public:
  WebFileHandlersTest() : channel_(version_info::Channel::BETA) {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionWebFileHandlers);
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
    EXPECT_EQ(base::Value::Type::DICT, manifest_value.type());
    return ManifestData(std::move(manifest_value).TakeDict());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  extensions::ScopedCurrentChannel channel_;
};

// `file_handlers` examples.
TEST_F(WebFileHandlersTest, GeneralSuccess) {
  struct {
    const char* title;
    const char* file_handler;
  } test_cases[] = {
      {
          "Minimum required entry.",
          R"([{
            "name": "Comma separated values",
            "action": "/open-csv",
            "accept": {"text/csv": ".csv"}
          }])",
      },
      {
          "Succeed with a different launch type and multiple icon sizes.",
          R"([{
            "name": "Comma separated values",
            "action": "/csv",
            "accept": {"text/csv": ".csv"},
            "icons": [{
              "src": "/csv.png",
              "sizes": "32x32 64x64",
              "type": "image/png"
            }],
            "launch_type": "multiple-clients"
          }])",
      },
      {
          "Succeed loading the example provlided in the file handling spec.",
          R"([
            {
              "action": "/open-csv",
              "name": "Comma-separated Value",
              "accept": {"text/csv": [ ".csv" ]},
              "icons": [{
                "src": "/csv-file.png",
                "sizes": "144x144"
              }]
            },
            {
              "action": "/open-svg",
              "name": "Image",
              "accept": {"image/svg+xml": ".svg"},
              "icons": [{
                "src": "/svg-file.png",
                "sizes": "144x144"
              }]
            },
            {
              "action": "/open-graf",
              "name": "Grafr",
              "accept": {
                "application/vnd.grafr.graph": [".grafr", ".graf"],
                "application/vnd.alternative-graph-app.graph": ".graph"
              },
              "launch_type": "multiple-clients"
            }
          ])",
      },
      {
          "Succeed with usage of all possible available keys.",
          R"([{
            "name": "Comma separated values",
            "action": "/csv",
            "accept": {"text/csv": ".csv"},
            "icons": [{
              "src": "/csv.png",
              "sizes": "16x16",
              "type": "image/png"
            }],
            "launch-type": "single-client"
          }])",
      },
      {
          "Succeed if `action` has a leading forward slash.",
          R"([{
            "name":"test",
            "action":"/path",
            "accept": {"text/csv": ".csv"}
          }])",
      },
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);

    // Load extension and get file handlers.
    scoped_refptr<Extension> extension(LoadAndExpectSuccess(
        std::move(GetManifestData(test_case.file_handler))));
    auto* file_handlers = WebFileHandlers::GetFileHandlers(*extension.get());
    ASSERT_TRUE(file_handlers);

    // Exercise the web `file_handlers` key with a subkey introduced in MV3.
    for (const auto& file_handler : *file_handlers) {
      EXPECT_TRUE(file_handler.file_handler.action.size() > 0);
    }
  }
}

// General usage verification.
TEST_F(WebFileHandlersTest, GeneralErrors) {
  struct {
    const char* title;
    const char* file_handler;
    const char* expected_error;
  } test_cases[] = {
      {
          "Error when accept is missing.",
          R"([{
            "name": "Comma separated values",
            "action": "/open-csv"
          }])",
          "Error at key 'file_handlers'. Parsing array failed at "
          "index 0: 'accept' is required",
      },
      {
          "Error when name is missing.",
          R"([{
            "action": "/open-csv",
            "accept": {"text/csv": ".csv"}
          }])",
          "Error at key 'file_handlers'. Parsing array failed at "
          "index 0: 'name' is required",
      },
      {
          "Error when action is missing.",
          R"([{
            "name": "Comma separated values",
            "accept": {"text/csv": ".csv"}
          }])",
          "Error at key 'file_handlers'. Parsing array failed at "
          "index 0: 'action' is required",
      },
      {
          "Error with an empty array.",
          R"([])",
          "Invalid value for 'file_handlers[0]'. At least one File "
          "Handler must be present.",
      },
      {
          "Error with an empty file handler.",
          R"([{}])",
          "Error at key 'file_handlers'. Parsing array failed at "
          "index 0: 'accept' is required",
      },
      {
          "Error with multiple missing file handlers.",
          R"([{}, {}])",
          "Error at key 'file_handlers'. Parsing array failed at "
          "index 0: 'accept' is required",
      },
      {
          "Error with an empty file handler.",
          R"([{"accept": {"text/csv": ".csv"}}])",
          "Error at key 'file_handlers'. Parsing array failed at "
          "index 0: 'action' is required",
      },
      {
          "Error if `name` is empty.",
          R"([{
            "name": "",
            "action": "",
            "accept": {"text/csv": ".csv"}
          }])",
          "Invalid value for 'file_handlers[0]'. `name` must have a value.",
      },
      {
          "Error if `action` is empty.",
          R"([{
            "name": "test",
            "action": "",
            "accept": {"text/csv": ".csv"}
          }])",
          "Invalid value for 'file_handlers[0]'. `action` must have a value.",
      },
      {
          "Error if `action` doesn't have a leading forward slash.",
          R"([{
            "name":"test",
            "action":"path",
            "accept": {"text/csv": ".csv"}
          }])",
          "Invalid value for 'file_handlers[0]'. `action` must "
          "start with a forward slash.",
      },
      {
          "Error if `launch_type` multiple-clients is singular.",
          R"([{
            "name":"test",
            "action":"/path",
            "accept": {"text/csv": ".csv"},
            "launch_type": "multiple-client"
          }])",
          "Invalid value for 'file_handlers[0]'. `launch_type` must have a "
          "valid value.",
      }};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);
    LoadAndExpectError(GetManifestData(test_case.file_handler),
                       test_case.expected_error);
  }
}

// `accept` property verification.
TEST_F(WebFileHandlersTest, AcceptErrors) {
  struct {
    const char* title;
    const char* file_handler;
    const char* expected_error;
  } test_cases[] = {
      {
          "Error if `accept` has a file extension that's missing the leading "
          "period.",
          R"([{
            "name": "test",
            "action": "/csv",
            "accept": {"text/csv": ["csv"]}
          }])",
          "Invalid value for 'file_handlers[0]'. `accept` file extension must "
          "have a leading period.",
      },
      {
          "Error if `accept` has a mime type in the wrong format.",
          R"([{
            "name": "test",
            "action": "/csv",
            "accept": {"csv": ".csv"}
          }])",
          "Invalid value for 'file_handlers[0]'. `accept` mime type "
          "must have exactly one slash.",
      },
      {
          "Error if `accept` has a mime type in the wrong format.",
          R"([{
            "name": "test",
            "action": "/csv",
            "accept": {"text//csv": ".csv"}
          }])",
          "Invalid value for 'file_handlers[0]'. `accept` mime type "
          "must have exactly one slash.",
      },
      {
          "`accept` must have the correct type to represent the file "
          "extension.",
          R"([{
            "name": "test",
            "action": "/csv",
            "accept": {"text/csv": {}}
          }])",
          "Invalid value for 'file_handlers[0]'. `accept` must have "
          "a valid file extension.",
      },
      {
          "Error if `accept` is empty.",
          R"([{
            "name": "test",
            "action": "/csv",
            "accept": {}
          }])",
          "Invalid value for 'file_handlers[0]'. `accept` cannot be empty.",
      },
      {
          "Error if `accept` is empty.",
          R"([{
            "name": "test",
            "action": "/csv",
            "accept": {"text/csv": []}
          }])",
          "Invalid value for 'file_handlers[0]'. `accept` file "
          "extension must have a value.",
      },
      {
          "Error if `accept` is empty.",
          R"([{
            "name": "test",
            "action": "/csv",
            "accept": {"text/csv": [""]}
          }])",
          "Invalid value for 'file_handlers[0]'. `accept` file "
          "extension must have a value.",
      }};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);
    LoadAndExpectError(GetManifestData(test_case.file_handler),
                       test_case.expected_error);
  }
}

// `icon` property verification.
TEST_F(WebFileHandlersTest, IconErrors) {
  struct {
    const char* title;
    const char* file_handler;
    const char* expected_error;
  } test_cases[] = {
      {
          "`icons.src` must have a value.",
          R"([{
            "name": "Comma separated values",
            "action": "/csv",
            "accept": {"text/csv": ".csv"},
            "icons": [{"src": ""}]
          }])",
          "Invalid value for 'file_handlers[0]'. `icon.src` must have a value.",
      },
      {
          "`icon.sizes` must have a value.",
          R"([{
            "name": "Comma separated values",
            "action": "/csv",
            "accept": {"text/csv": ".csv"},
            "icons": [{
              "src": "/csv.png",
              "sizes": "",
              "type": "image/png"
            }]
          }])",
          "Invalid value for 'file_handlers[0]'. `icon.sizes` must have a "
          "value.",
      },
      {
          "`icon.sizes` must have width and height.",
          R"([{
            "name": "Comma separated values",
            "action": "/csv",
            "accept": {"text/csv": ".csv"},
            "icons": [{
              "src": "/csv.png",
              "sizes": "16",
              "type": "image/png"
            }]
          }])",
          "Invalid value for 'file_handlers[0]'. `icon.sizes` must "
          "have width and height.",
      },
      {
          "`icon.sizes` dimensions must be digits.",
          R"([{
            "name": "Comma separated values",
            "action": "/csv",
            "accept": {"text/csv": ".csv"},
            "icons": [{
              "src": "/csv.png",
              "sizes": "a1x2",
              "type": "image/png"
            }]
          }])",
          "Invalid value for 'file_handlers[0]'. `icon.sizes` "
          "dimensions must be digits.",
      }};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);
    LoadAndExpectError(GetManifestData(test_case.file_handler),
                       test_case.expected_error);
  }
}

// TODO(crbug.com/40169582): Add tests for MV2, MV3, and missing the flag.
// crrev.com/c/4215992/comment/5c5148e7_2b24c9d3

}  // namespace extensions
