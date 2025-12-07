// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(MIMETypeRegistryTest, MimeTypeTest) {
  EXPECT_TRUE(MIMETypeRegistry::IsSupportedImagePrefixedMIMEType("image/gif"));
  EXPECT_TRUE(MIMETypeRegistry::IsSupportedImageResourceMIMEType("image/gif"));
  EXPECT_TRUE(MIMETypeRegistry::IsSupportedImagePrefixedMIMEType("Image/Gif"));
  EXPECT_TRUE(MIMETypeRegistry::IsSupportedImageResourceMIMEType("Image/Gif"));
  static const UChar kUpper16[] = {0x0049, 0x006d, 0x0061, 0x0067,
                                   0x0065, 0x002f, 0x0067, 0x0069,
                                   0x0066, 0};  // Image/gif in UTF16
  EXPECT_TRUE(
      MIMETypeRegistry::IsSupportedImagePrefixedMIMEType(String(kUpper16)));
  EXPECT_TRUE(
      MIMETypeRegistry::IsSupportedImagePrefixedMIMEType("image/svg+xml"));
  EXPECT_FALSE(
      MIMETypeRegistry::IsSupportedImageResourceMIMEType("image/svg+xml"));
}

TEST(MIMETypeRegistryTest, PluginMimeTypes) {
  // Since we've removed MIME type guessing based on plugin-declared file
  // extensions, ensure that the MIMETypeRegistry already contains
  // the extensions used by common PPAPI plugins.
  EXPECT_EQ("application/pdf",
            MIMETypeRegistry::GetWellKnownMIMETypeForExtension("pdf").Utf8());
  EXPECT_EQ("application/x-shockwave-flash",
            MIMETypeRegistry::GetWellKnownMIMETypeForExtension("swf").Utf8());
}

TEST(MIMETypeRegistryTest, PlainTextMIMEType) {
  EXPECT_TRUE(MIMETypeRegistry::IsPlainTextMIMEType("text/plain"));
  EXPECT_TRUE(MIMETypeRegistry::IsPlainTextMIMEType("text/javascript"));
  EXPECT_TRUE(MIMETypeRegistry::IsPlainTextMIMEType("TEXT/JavaScript"));
  EXPECT_FALSE(MIMETypeRegistry::IsPlainTextMIMEType("text/html"));
  EXPECT_FALSE(MIMETypeRegistry::IsPlainTextMIMEType("text/xml"));
  EXPECT_FALSE(MIMETypeRegistry::IsPlainTextMIMEType("text/xsl"));
}

TEST(MIMETypeRegistryTest, TextXMLType) {
  EXPECT_TRUE(MIMETypeRegistry::IsXMLMIMEType("text/xml"));
  EXPECT_TRUE(MIMETypeRegistry::IsXMLMIMEType("Text/xml"));
  EXPECT_TRUE(MIMETypeRegistry::IsXMLMIMEType("tEXt/XML"));
  EXPECT_TRUE(MIMETypeRegistry::IsXMLMIMEType("application/xml"));
  EXPECT_TRUE(MIMETypeRegistry::IsXMLMIMEType("application/XML"));
  EXPECT_TRUE(MIMETypeRegistry::IsXMLMIMEType("application/x-tra+xML"));
  EXPECT_TRUE(MIMETypeRegistry::IsXMLMIMEType("application/xslt+xml"));
  EXPECT_TRUE(MIMETypeRegistry::IsXMLMIMEType("application/rdf+Xml"));
  EXPECT_TRUE(MIMETypeRegistry::IsXMLMIMEType("image/svg+xml"));
  EXPECT_TRUE(MIMETypeRegistry::IsXMLMIMEType("application/x+xml"));

  EXPECT_FALSE(MIMETypeRegistry::IsXMLMIMEType("application/x-custom;a=a+xml"));
  EXPECT_FALSE(
      MIMETypeRegistry::IsXMLMIMEType("application/x-custom;a=a+xml ;"));
  EXPECT_FALSE(MIMETypeRegistry::IsXMLMIMEType("application/x-custom+xml2"));
  EXPECT_FALSE(MIMETypeRegistry::IsXMLMIMEType("application/x-custom+xml2  "));
  EXPECT_FALSE(MIMETypeRegistry::IsXMLMIMEType("application/x-custom+exml"));
  EXPECT_FALSE(MIMETypeRegistry::IsXMLMIMEType("text/html"));
  EXPECT_FALSE(MIMETypeRegistry::IsXMLMIMEType("application/xml;"));
  EXPECT_FALSE(MIMETypeRegistry::IsXMLMIMEType("application/xml "));
  EXPECT_FALSE(MIMETypeRegistry::IsXMLMIMEType("application/x-what+xml;"));
  EXPECT_FALSE(MIMETypeRegistry::IsXMLMIMEType("application/x-tra+xML;a=2"));
  EXPECT_FALSE(MIMETypeRegistry::IsXMLMIMEType("application/+xML"));
  EXPECT_FALSE(MIMETypeRegistry::IsXMLMIMEType("application/+xml"));
  EXPECT_FALSE(MIMETypeRegistry::IsXMLMIMEType("text/xsl"));
  EXPECT_FALSE(MIMETypeRegistry::IsXMLMIMEType("text/XSL"));
}

TEST(MIMETypeRegistryTest, XMLExternalEntityMIMEType) {
  EXPECT_TRUE(MIMETypeRegistry::IsXMLExternalEntityMIMEType(
      "application/xml-external-parsed-entity"));
  EXPECT_TRUE(MIMETypeRegistry::IsXMLExternalEntityMIMEType(
      "text/xml-external-parsed-entity"));
  EXPECT_TRUE(MIMETypeRegistry::IsXMLExternalEntityMIMEType(
      "application/XML-external-parsed-entity"));
  EXPECT_TRUE(MIMETypeRegistry::IsXMLExternalEntityMIMEType(
      "text/XML-external-parsed-entity"));

  EXPECT_FALSE(MIMETypeRegistry::IsXMLExternalEntityMIMEType("text/plain"));
  EXPECT_FALSE(MIMETypeRegistry::IsXMLExternalEntityMIMEType("text/html"));
  EXPECT_FALSE(MIMETypeRegistry::IsXMLExternalEntityMIMEType("text/xml"));
}

}  // namespace blink
