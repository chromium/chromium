// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_type_converters.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink.h"
#include "third_party/blink/renderer/modules/manifest/manifest_parser.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

class ManifestTypeConvertersTest : public testing::Test {
 protected:
  ManifestTypeConvertersTest() {}
  ~ManifestTypeConvertersTest() override {}

  mojom::blink::ManifestPtr Load(const String& json) {
    KURL url("http://example.com");
    ManifestParser parser(json, url, url);
    parser.Parse();

    Vector<mojom::blink::ManifestErrorPtr> errors;
    parser.TakeErrors(&errors);

    EXPECT_EQ(0u, errors.size());
    EXPECT_FALSE(parser.failed());

    return parser.manifest().Clone();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ManifestTypeConvertersTest);
};

TEST_F(ManifestTypeConvertersTest, NoFileHandlerDoesNotConvert) {
  const String json = "{\"start_url\": \"/\"}";
  const mojom::blink::ManifestPtr& mojo_manifest = Load(json);

  auto manifest = mojo_manifest.To<blink::Manifest>();
  EXPECT_FALSE(manifest.file_handler.has_value());
}

TEST_F(ManifestTypeConvertersTest, BasicFileHandlerIsCorrectlyConverted) {
  const mojom::blink::ManifestPtr& mojo_manifest = Load(
      "{"
      "  \"file_handler\": {"
      "    \"files\": ["
      "      {"
      "        \"name\": \"name\", "
      "        \"accept\": \"image/png\""
      "      }"
      "    ], "
      "    \"action\": \"/files\""
      "  }"
      "}");

  auto manifest = mojo_manifest.To<blink::Manifest>();
  ASSERT_TRUE(manifest.file_handler.has_value());

  EXPECT_EQ(manifest.file_handler->action, "http://example.com/files");
  ASSERT_EQ(manifest.file_handler->files.size(), 1u);
  EXPECT_TRUE(base::EqualsASCII(manifest.file_handler->files[0].name, "name"));
  ASSERT_EQ(manifest.file_handler->files[0].accept.size(), 1u);
  EXPECT_TRUE(base::EqualsASCII(manifest.file_handler->files[0].accept[0],
                                "image/png"));
}
}  // namespace blink
