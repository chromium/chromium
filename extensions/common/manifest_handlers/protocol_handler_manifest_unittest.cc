// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "components/version_info/channel.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/protocol_handler_info.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

class ManifestProtocolHandlersTest : public ManifestTest {
 public:
  ManifestProtocolHandlersTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionProtocolHandlers);
  }

 protected:
  ManifestData GetManifestData(const char* manifest_part) {
    static constexpr char kManifestStub[] =
        R"({
          "name": "Test",
          "version": "0.0.1",
          "manifest_version": 3,
          "protocol_handlers": %s
        })";
    base::Value manifest_value =
        base::test::ParseJson(base::StringPrintf(kManifestStub, manifest_part));
    EXPECT_EQ(base::Value::Type::DICT, manifest_value.type());
    return ManifestData(std::move(manifest_value).TakeDict());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedCurrentChannel current_channel_{version_info::Channel::DEV};
};

TEST_F(ManifestProtocolHandlersTest, GeneralSuccess) {
  struct {
    const char* title;
    const char* protocol_handler;
    const size_t expected_handlers_count;
  } test_cases[] = {
      {"Minimum required entry.",
       R"([
            {
              "protocol": "web+testingScheme",
              "name": "Testing handler",
              "uriTemplate": "https:/example.com/%s"
            }
          ])",
       1},
      {"Multiple protocols.",
       R"([
            {
              "protocol": "web+testingScheme",
              "name": "Testing handler 1",
              "uriTemplate": "https:/example1.com/%s"
            },
            {
              "protocol": "ipfs",
              "name": "Testing handler 2",
              "uriTemplate": "https:/example2.com/%s"
            }
          ])",
       2},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);

    scoped_refptr<Extension> extension(LoadAndExpectSuccess(
        std::move(GetManifestData(test_case.protocol_handler))));
    ASSERT_TRUE(extension);

    const ProtocolHandlersInfo* handlers =
        ProtocolHandlers::GetProtocolHandlers(*extension);
    ASSERT_TRUE(handlers);
    ASSERT_EQ(test_case.expected_handlers_count, handlers->size());
  }
}

TEST_F(ManifestProtocolHandlersTest, InvalidManifestVersion) {
  static constexpr char kManifestStub[] =
      R"({
          "name": "Test",
          "version": "0.0.1",
          "manifest_version": 2,
          "protocol_handlers": [
            {
             "protocol": "testingScheme",
              "name": "Testing handler",
              "uriTemplate": "testingURI"
             }
           ]
        })";

  base::Value manifest_value = base::test::ParseJson(kManifestStub);
  auto manifest_data = ManifestData(std::move(manifest_value).TakeDict());

  LoadAndExpectWarning(
      manifest_data,
      "'protocol_handlers' requires manifest version of at least 3.");
}

TEST_F(ManifestProtocolHandlersTest, InvalidManifestSyntax) {
  struct {
    const char* title;
    const char* protocol_handler;
    std::vector<std::string> expected_warnings;
  } test_cases[] = {
      {
          "Invalid declaration: empty list.",
          R"([])",
          {errors::kInvalidProtocolHandlersEmpty},
      },
      // protocol invalid cases.
      {
          "Missing 'protocol' key: not declared.",
          R"([
            {
              "name": "Testing handler",
              "uriTemplate": "testingURI"
            }
          ])",
          {"Error at key 'protocol_handlers'. Parsing array failed at index 0: "
           "'protocol' is required"},
      },
      {
          "Missing 'name' key: not declared.",
          R"([
            {
              "protocol": "web+testingScheme",
              "uriTemplate": "testingURI"
            }
          ])",
          {"Error at key 'protocol_handlers'. Parsing array failed at index 0: "
           "'name' is required"},
      },
      {
          "Missing protocol key: wrong key.",
          R"([
            {
              "protocol": "testingScheme",
              "name": "Testing handler 1",
              "uriTemplate": "testingURI"
            },
            {
              "scheme": "testingScheme",
              "name": "Testing handler 2",
              "uriTemplate": "testingURI"
            }
          ])",
          {"Error at key 'protocol_handlers'. Parsing array failed at index 1: "
           "'protocol' is required"},
      },
      {
          "Missing uriTemplate key: not declared.",
          R"([
            {
              "name": "Testing handler 1",
              "protocol": "testingScheme",
            }
          ])",
          {"Error at key 'protocol_handlers'. Parsing array failed at index 0: "
           "'uriTemplate' is required"},
      },
      {
          "Missing uriTemplate key: wrong key.",
          R"([
            {
              "protocol": "testingURI",
              "name": "Testing handler 1",
              "uriTemplate": "testingScheme"
            },
            {
              "protocol": "testingScheme",
              "name": "Testing handler 1",
              "url": "testingURI"
            }
          ])",
          {"Error at key 'protocol_handlers'. Parsing array failed at index 1: "
           "'uriTemplate' is required"},
      },
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);
    LoadAndExpectWarnings(GetManifestData(test_case.protocol_handler),
                          test_case.expected_warnings);
  }
}

TEST_F(ManifestProtocolHandlersTest, InvalidProtocolHandler) {
  struct {
    const char* title;
    const char* protocol_handler;
    std::vector<std::string> expected_warnings;
  } test_cases[] = {
      {
          "Registering handler for an unprefix scheme.",
          R"([
            {
              "protocol": "glsearch",
              "name": "Testing handler",
              "uriTemplate": "https://www.google.com/search?q=%s"
            }
          ])",
          {errors::kProtocolHandlerSchemeNotInSafeList},
      },
      {
          "Protocol handlers with unprefix scheme and an empty name.",
          R"([
            {
              "protocol": "glsearch",
              "name": "",
              "uriTemplate": "https://www.google.com/search?q=%s"
            }
          ])",
          {errors::kProtocolHandlerEmptyName,
           errors::kProtocolHandlerSchemeNotInSafeList},
      },
      {
          "Registering handler for a reserved scheme.",
          R"([
            {
              "protocol": "https",
              "name": "Testing handler",
              "uriTemplate": "https://www.google.com/search?q=%s"
            }
          ])",
          {errors::kProtocolHandlerSchemeNotInSafeList},
      },
      {
          "Custom handler URL without '%s' token.",
          R"([
            {
              "protocol": "ssh",
              "name": "Testing handler",
              "uriTemplate": "https://www.google.com/search"
            }
          ])",
          {errors::kProtocolHandlerUrlTokenMissing},
      },
      {
          "Custom handler URL with an invalid syntax.",
          R"([
            {
              "protocol": "web+glsearch",
              "name": "Testing handler",
              "uriTemplate": "https:/example.com:abc/path"
            }
          ])",
          {errors::kProtocolHandlerUrlInvalidSyntax},
      },
      {
          "Custom handler URL with an untrustworthy scheme.",
          R"([
            {
              "protocol": "web+glsearch",
              "name": "Testing handler",
              "uriTemplate": "http:/example.com/%s"
            }
          ])",
          {errors::kProtocolHandlerUntrustworthyScheme},
      },
      {
          "Custom handler URL with an untrustworthy scheme and an Opaque "
          "Origin.",
          R"([
            {
              "protocol": "web+glsearch",
              "name": "Testing handler",
              "uriTemplate": "data:/%s"
            }
          ])",
          {errors::kProtocolHandlerUntrustworthyScheme,
           errors::kProtocolHandlerOpaqueOrigin},
      },
      {
          "Custom handler URL with an invalid syntax and untrustworthy scheme.",
          R"([
            {
              "protocol": "web+glsearch",
              "name": "Testing handler",
              "uriTemplate": "http://[v8.:::]//url=%s"
            }
          ])",
          {errors::kProtocolHandlerUrlInvalidSyntax,
           errors::kProtocolHandlerUntrustworthyScheme},
      },
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);
    LoadAndExpectWarnings(GetManifestData(test_case.protocol_handler),
                          test_case.expected_warnings);
  }
}

}  // namespace extensions
