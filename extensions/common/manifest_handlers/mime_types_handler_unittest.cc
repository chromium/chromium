// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/mime_types_handler.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/test/values_test_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using ::testing::ElementsAre;

using MimeTypesHandlerNotAllowedTest = ManifestTest;

class MimeTypesHandlerTest : public ManifestTest {
 protected:
  ExtensionId GetTestExtensionID() const override {
    // Extension ID must correspond to a hashed extension ID in the allowlist
    // for "mime_types" and "mime_types_handler" in _manifest_features.json.
    return extension_misc::kMimeHandlerPrivateTestExtensionId;
  }
};

}  // namespace

TEST_F(MimeTypesHandlerNotAllowedTest, Load) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData(base::test::ParseJson(R"({
        "name": "Test Extension",
        "manifest_version": 3,
        "version": "0.1",
        "mime_types": ["text/plain", "application/octet-stream"],
        "mime_types_handler": "index.html"
      })")
                                            .TakeDict()));
  ASSERT_TRUE(extension);

  EXPECT_FALSE(MimeTypesHandler::GetHandler(extension.get()));
}

TEST_F(MimeTypesHandlerTest, Load) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData(base::test::ParseJson(R"({
        "name": "Test Extension",
        "manifest_version": 3,
        "version": "0.1",
        "mime_types": ["text/plain", "application/octet-stream"],
        "mime_types_handler": "index.html"
      })")
                                            .TakeDict()));
  ASSERT_TRUE(extension);

  MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension.get());
  ASSERT_TRUE(handler);

  EXPECT_THAT(handler->mime_type_set(),
              ElementsAre("application/octet-stream", "text/plain"));
}

}  // namespace extensions
