// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_type_converters.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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
  EXPECT_EQ(0u, manifest.file_handlers.size());
}

TEST_F(ManifestTypeConvertersTest, BasicFileHandlerIsCorrectlyConverted) {
  const mojom::blink::ManifestPtr& mojo_manifest = Load(
      "{"
      "  \"file_handlers\": ["
      "    {"
      "      \"name\": \"name\","
      "      \"action\": \"/files\","
      "      \"accept\": {"
      "        \"image/png\": ["
      "          \".png\""
      "        ]"
      "      }"
      "    }"
      "  ]"
      "}");

  auto manifest = mojo_manifest.To<blink::Manifest>();
  ASSERT_EQ(manifest.file_handlers.size(), 1u);
  EXPECT_EQ(manifest.file_handlers[0].action, "http://example.com/files");
  EXPECT_TRUE(base::EqualsASCII(manifest.file_handlers[0].name, "name"));
  ASSERT_EQ(manifest.file_handlers[0].accept.size(), 1u);

  base::string16 mime = base::UTF8ToUTF16("image/png");
  ASSERT_EQ(manifest.file_handlers[0].accept.count(mime), 1u);
  EXPECT_EQ(manifest.file_handlers[0].accept[mime].size(), 1u);
  EXPECT_TRUE(
      base::EqualsASCII(manifest.file_handlers[0].accept[mime][0], ".png"));
}

TEST_F(ManifestTypeConvertersTest, NoShortcutsDoesNotConvert) {
  const String json = "{\"start_url\": \"/\"}";
  const mojom::blink::ManifestPtr& mojo_manifest = Load(json);

  auto manifest = mojo_manifest.To<blink::Manifest>();
  EXPECT_TRUE(manifest.shortcuts.empty());
}

TEST_F(ManifestTypeConvertersTest, BasicShortcutIsCorrectlyConverted) {
  const mojom::blink::ManifestPtr& mojo_manifest = Load(
      "{"
      "  \"shortcuts\": ["
      "      {"
      "        \"name\": \"name\", "
      "        \"short_name\": \"short_name\","
      "        \"url\": \"url\", "
      "        \"icons\": ["
      "            {"
      "              \"src\": \"image.jpg\""
      "            }"
      "          ] "
      "      }"
      "    ] "
      "}");

  auto manifest = mojo_manifest.To<blink::Manifest>();
  ASSERT_FALSE(manifest.shortcuts.empty());

  ASSERT_EQ(manifest.shortcuts.size(), 1u);
  EXPECT_TRUE(base::EqualsASCII(manifest.shortcuts[0].name, "name"));
  EXPECT_EQ(manifest.shortcuts[0].short_name, base::ASCIIToUTF16("short_name"));
  EXPECT_EQ(manifest.shortcuts[0].url.spec(), "http://example.com/url");

  ASSERT_EQ(manifest.shortcuts[0].icons.size(), 1u);
  ASSERT_EQ(manifest.shortcuts[0].icons[0].src.spec(),
            "http://example.com/image.jpg");
}

}  // namespace blink
