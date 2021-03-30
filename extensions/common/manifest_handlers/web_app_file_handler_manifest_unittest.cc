// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/web_app_file_handler.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

class WebAppFileHandlersManifestTest : public ManifestTest {
 protected:
  ManifestData CreateManifest(const char* web_app_file_handlers) {
    const char kManifestTemplate[] =
        R"({
          "name": "test",
          "manifest_version": 2,
          "version": "1",
          "app": {
            "launch": {
              "web_url": "https://api.site"
            }
          },
          "web_app_file_handlers": %s
        })";
    base::Value manifest = base::test::ParseJson(
        base::StringPrintf(kManifestTemplate, web_app_file_handlers));
    return ManifestData(std::move(manifest), "test");
  }
};

}  // namespace

// Non-list type for "web_app_file_handlers" throws an error.
TEST_F(WebAppFileHandlersManifestTest, InvalidFileHandlers) {
  const char kFileHandlers[] = R"({})";
  LoadAndExpectError(
      CreateManifest(kFileHandlers), errors::kInvalidWebAppFileHandlers,
      ManifestLocation::kInternal, extensions::Extension::FROM_BOOKMARK);
}

// Non-dictionary type for "web_app_file_handlers[*]" throws an error.
TEST_F(WebAppFileHandlersManifestTest, InvalidFileHandler) {
  const char kFileHandlers[] = R"(["foo"])";
  LoadAndExpectError(
      CreateManifest(kFileHandlers), errors::kInvalidWebAppFileHandler,
      ManifestLocation::kInternal, extensions::Extension::FROM_BOOKMARK);
}

// Non-string type for "web_app_file_handlers[*].action" throws an error.
TEST_F(WebAppFileHandlersManifestTest, InvalidFileHandlerActionNonString) {
  const char kFileHandlers[] = R"([
    {
      "action": "https://api.site/open-foo",
      "accept": {
        "application/foo": ".foo",
        "application/foobar": [".foobar"]
      }
    },
    {
      "action": 42,
      "accept": {
        "application/bar": [
          ".bar",
          ".baz"
        ]
      }
    }
  ])";
  LoadAndExpectError(
      CreateManifest(kFileHandlers), errors::kInvalidWebAppFileHandlerAction,
      ManifestLocation::kInternal, extensions::Extension::FROM_BOOKMARK);
}

// Invalid GURL in "web_app_file_handlers[*].action" throws an error.
TEST_F(WebAppFileHandlersManifestTest, InvalidFileHandlerActionInvalidGURL) {
  const char kFileHandlers[] = R"([
    {
      "action": "https://api.site/open-foo",
      "accept": {
        "application/foo": ".foo",
        "application/foobar": [".foobar"]
      }
    },
    {
      "action": "/open-bar",
      "accept": {
        "application/bar": [
          ".bar",
          ".baz"
        ]
      }
    }
  ])";
  LoadAndExpectError(
      CreateManifest(kFileHandlers), errors::kInvalidWebAppFileHandlerAction,
      ManifestLocation::kInternal, extensions::Extension::FROM_BOOKMARK);
}

// Non-dictionary type for "web_app_file_handlers[*].accept" throws an error.
TEST_F(WebAppFileHandlersManifestTest, InvalidFileHandlerAccept) {
  const char kFileHandlers[] = R"([
    {
      "action": "https://api.site/open-foo",
      "accept": {
        "application/foo": ".foo",
        "application/foobar": [".foobar"]
      }
    },
    {
      "action": "https://api.site/open-bar",
      "accept": [
        ".bar",
        ".baz"
      ]
    }
  ])";
  LoadAndExpectError(
      CreateManifest(kFileHandlers), errors::kInvalidWebAppFileHandlerAccept,
      ManifestLocation::kInternal, extensions::Extension::FROM_BOOKMARK);
}

// Empty dictionary in "web_app_file_handlers[*].accept" throws an error.
TEST_F(WebAppFileHandlersManifestTest, InvalidFileHandlerEmptyAccept) {
  const char kFileHandlers[] = R"([
    {
      "action": "https://api.site/open-foo",
      "accept": {
        "application/foo": ".foo",
        "application/foobar": [".foobar"]
      }
    },
    {
      "action": "https://api.site/open-bar",
      "accept": {}
    }
  ])";
  LoadAndExpectError(CreateManifest(kFileHandlers),
                     errors::kInvalidWebAppFileHandlerEmptyAccept,
                     ManifestLocation::kInternal,
                     extensions::Extension::FROM_BOOKMARK);
}

// Non-list type for "web_app_file_handlers[*].accept[*]" throws an error.
TEST_F(WebAppFileHandlersManifestTest, InvalidFileHandlerFileExtensions) {
  const char kFileHandlers[] = R"([
    {
      "action": "https://api.site/open-foo",
      "accept": {
        "application/foo": ".foo",
        "application/foobar": [".foobar"]
      }
    },
    {
      "action": "https://api.site/open-bar",
      "accept": {
        "application/bar": 42
      }
    }
  ])";
  LoadAndExpectError(CreateManifest(kFileHandlers),
                     errors::kInvalidWebAppFileHandlerFileExtensions,
                     ManifestLocation::kInternal,
                     extensions::Extension::FROM_BOOKMARK);
}

// Non-string type for element in "web_app_file_handlers[*].accept[*]" throws an
// error.
TEST_F(WebAppFileHandlersManifestTest,
       InvalidFileHandlerFileExtensionNotAString) {
  const char kFileHandlers[] = R"([
    {
      "action": "https://api.site/open-foo",
      "accept": {
        "application/foo": ".foo",
        "application/foobar": [".foobar"]
      }
    },
    {
      "action": "https://api.site/open-bar",
      "accept": {
        "application/bar": [
          ".bar",
          42
        ]
      }
    }
  ])";
  LoadAndExpectError(CreateManifest(kFileHandlers),
                     errors::kInvalidWebAppFileHandlerFileExtension,
                     ManifestLocation::kInternal,
                     extensions::Extension::FROM_BOOKMARK);
}

// Empty string for element in "web_app_file_handlers[*].accept[*]" throws an
// error.
TEST_F(WebAppFileHandlersManifestTest, InvalidFileHandlerFileExtensionEmpty) {
  const char kFileHandlers[] = R"([
    {
      "action": "https://api.site/open-foo",
      "accept": {
        "application/foo": ".foo",
        "application/foobar": [".foobar"]
      }
    },
    {
      "action": "https://api.site/open-bar",
      "accept": {
        "application/bar": [
          ".bar",
          ""
        ]
      }
    }
  ])";
  LoadAndExpectError(CreateManifest(kFileHandlers),
                     errors::kInvalidWebAppFileHandlerFileExtension,
                     ManifestLocation::kInternal,
                     extensions::Extension::FROM_BOOKMARK);
}

// Missing '.' prefix for element in "web_app_file_handlers[*].accept[*]" throws
// an error.
TEST_F(WebAppFileHandlersManifestTest, InvalidFileHandlerFileExtensionNoDot) {
  const char kFileHandlers[] = R"([
    {
      "action": "https://api.site/open-foo",
      "accept": {
        "application/foo": ".foo",
        "application/foobar": [".foobar"]
      }
    },
    {
      "action": "https://api.site/open-bar",
      "accept": {
        "application/bar": [
          ".bar",
          "baz"
        ]
      }
    }
  ])";
  LoadAndExpectError(CreateManifest(kFileHandlers),
                     errors::kInvalidWebAppFileHandlerFileExtension,
                     ManifestLocation::kInternal,
                     extensions::Extension::FROM_BOOKMARK);
}

// Valid "web_app_file_handlers" manifest entry.
TEST_F(WebAppFileHandlersManifestTest, ValidFileHandlers) {
  const char kFileHandlers[] = R"([
    {
      "action": "https://api.site/open-foo",
      "accept": {
        "application/foo": ".foo",
        "application/foobar": [".foobar"]
      }
    },
    {
      "action": "https://api.site/open-bar",
      "accept": {
        "application/bar": [
          ".bar",
          ".baz"
        ]
      }
    }
  ])";
  scoped_refptr<const Extension> extension = LoadAndExpectSuccess(
      CreateManifest(kFileHandlers), ManifestLocation::kInternal,
      extensions::Extension::FROM_BOOKMARK);

  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->from_bookmark());

  const apps::FileHandlers* file_handlers =
      WebAppFileHandlers::GetWebAppFileHandlers(extension.get());
  ASSERT_TRUE(file_handlers);
  ASSERT_EQ(2u, file_handlers->size());

  {
    apps::FileHandler file_handler = file_handlers->at(0);
    EXPECT_EQ("https://api.site/open-foo", file_handler.action);
    EXPECT_EQ(2u, file_handler.accept.size());
    EXPECT_EQ("application/foo", file_handler.accept[0].mime_type);
    EXPECT_THAT(file_handler.accept[0].file_extensions,
                testing::UnorderedElementsAre(".foo"));
    EXPECT_EQ("application/foobar", file_handler.accept[1].mime_type);
    EXPECT_THAT(file_handler.accept[1].file_extensions,
                testing::UnorderedElementsAre(".foobar"));
  }

  {
    apps::FileHandler file_handler = file_handlers->at(1);
    EXPECT_EQ("https://api.site/open-bar", file_handler.action);
    EXPECT_EQ(1u, file_handler.accept.size());
    EXPECT_EQ("application/bar", file_handler.accept[0].mime_type);
    EXPECT_THAT(file_handler.accept[0].file_extensions,
                testing::UnorderedElementsAre(".bar", ".baz"));
  }
}

// The "web_app_file_handlers" key is only available for Bookmark Apps.
// Here we expect the Extension to load successfully, but that the manifest
// parser to return a nullptr, and that an install warning is present.
TEST_F(WebAppFileHandlersManifestTest, NotBookmarkApp) {
  const char kFileHandlers[] = R"([
    {
      "action": "https://api.site/open-foo",
      "accept": {
        "application/foo": ".foo",
        "application/foobar": [".foobar"]
      }
    },
    {
      "action": "https://api.site/open-bar",
      "accept": {
        "application/bar": [
          ".bar",
          ".baz"
        ]
      }
    }
  ])";
  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess(CreateManifest(kFileHandlers));

  ASSERT_TRUE(extension.get());
  ASSERT_FALSE(extension->from_bookmark());

  std::vector<InstallWarning> expected_install_warnings;
  expected_install_warnings.push_back(
      InstallWarning(errors::kInvalidWebAppFileHandlersNotBookmarkApp));
  EXPECT_EQ(expected_install_warnings, extension->install_warnings());

  const apps::FileHandlers* file_handlers =
      WebAppFileHandlers::GetWebAppFileHandlers(extension.get());
  ASSERT_FALSE(file_handlers);
}

}  // namespace extensions
