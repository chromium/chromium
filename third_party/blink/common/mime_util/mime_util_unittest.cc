// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mime_util/mime_util.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "net/base/mime_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

TEST(MimeUtilJsonMimeTypeTest, IsJSON) {
  EXPECT_TRUE(IsJSONMimeType("application/json"));
  EXPECT_TRUE(IsJSONMimeType("text/json"));
  EXPECT_TRUE(IsJSONMimeType("application/blah+json"));
  EXPECT_TRUE(IsJSONMimeType("Application/JSON"));
  EXPECT_TRUE(IsJSONMimeType("Text/JSON"));
  EXPECT_TRUE(IsJSONMimeType("application/json;x=1"));
  EXPECT_TRUE(IsJSONMimeType("application/blah+json;x=1"));
  EXPECT_TRUE(IsJSONMimeType("text/json;x=1"));

  EXPECT_FALSE(IsJSONMimeType("json"));
  EXPECT_FALSE(IsJSONMimeType("+json"));
  EXPECT_FALSE(IsJSONMimeType("application/"));
  EXPECT_FALSE(IsJSONMimeType("application/jsonabcd"));
  EXPECT_FALSE(IsJSONMimeType("application/blahjson"));
  EXPECT_FALSE(IsJSONMimeType("application/blah+jsonabcd"));
  EXPECT_FALSE(IsJSONMimeType("application/foo+json bar"));
  EXPECT_FALSE(IsJSONMimeType("application/foo+jsonbar;a=b"));
  EXPECT_FALSE(IsJSONMimeType("application/json+blah"));
  EXPECT_FALSE(IsJSONMimeType("application/problem+"));
  EXPECT_FALSE(IsJSONMimeType("application/+"));
  EXPECT_FALSE(IsJSONMimeType("text/html;+json"));
  EXPECT_FALSE(IsJSONMimeType("text/html+json+xml"));
  EXPECT_FALSE(IsJSONMimeType("text/json/json"));

  EXPECT_TRUE(IsJSONMimeType("text/blah+json;x=1"));
  EXPECT_TRUE(IsJSONMimeType("text/html+json"));
  EXPECT_TRUE(IsJSONMimeType("image/svg+json"));

  EXPECT_TRUE(IsJSONMimeType("text/hal+json"));
  EXPECT_FALSE(IsJSONMimeType("te xt/hal+json"));
  EXPECT_FALSE(IsJSONMimeType("text/ha l+json"));
  EXPECT_FALSE(IsJSONMimeType("aplicación/hal+json"));
  EXPECT_FALSE(IsJSONMimeType("text/halé+json"));
}

class MimeUtilXmlMimeTypeTest : public testing::TestWithParam<bool> {};

TEST_P(MimeUtilXmlMimeTypeTest, IsXML) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(blink::features::kSpecCompliantXmlMimeTypes,
                                GetParam());
  const bool spec_compliant = GetParam();

  EXPECT_TRUE(IsXMLMimeType("text/xml"));
  EXPECT_TRUE(IsXMLMimeType("application/xml"));
  EXPECT_TRUE(IsXMLMimeType("application/atom+xml"));
  EXPECT_TRUE(IsXMLMimeType("Application/XML"));
  EXPECT_TRUE(IsXMLMimeType("Text/XML"));
  EXPECT_TRUE(IsXMLMimeType("application/xml;x=1"));
  EXPECT_TRUE(IsXMLMimeType("application/atom+xml;x=1"));

  EXPECT_EQ(IsXMLMimeType("text/+xml"), spec_compliant);
  EXPECT_EQ(IsXMLMimeType("text/html+xml"), spec_compliant);
  EXPECT_EQ(IsXMLMimeType("image/svg+xml"), spec_compliant);
  EXPECT_EQ(IsXMLMimeType("text/blah+xml;x=1"), spec_compliant);
  EXPECT_EQ(IsXMLMimeType("image/svg+xml;x=1"), spec_compliant);

  EXPECT_FALSE(IsXMLMimeType("xml"));
  EXPECT_FALSE(IsXMLMimeType("+xml"));
  EXPECT_FALSE(IsXMLMimeType("text+xml"));
  EXPECT_FALSE(IsXMLMimeType("application/"));
  EXPECT_FALSE(IsXMLMimeType("application/xmlabcd"));
  EXPECT_FALSE(IsXMLMimeType("application/blahxml"));
  EXPECT_FALSE(IsXMLMimeType("application/blah+xmlabcd"));
  EXPECT_FALSE(IsXMLMimeType("application/foo+xml bar"));
  EXPECT_FALSE(IsXMLMimeType("application/foo+xmlbar;a=b"));
  EXPECT_FALSE(IsXMLMimeType("application/xml+blah"));
  EXPECT_FALSE(IsXMLMimeType("application/problem+"));
  EXPECT_FALSE(IsXMLMimeType("application/+"));
  EXPECT_FALSE(IsXMLMimeType("text/html;+xml"));
  EXPECT_FALSE(IsXMLMimeType("text/html+xml+json"));
  EXPECT_FALSE(IsXMLMimeType("text/xml/xml"));
  EXPECT_FALSE(IsXMLMimeType("text/xsl"));

  EXPECT_FALSE(IsXMLMimeType("te xt/hal+xml"));
  EXPECT_FALSE(IsXMLMimeType("text/ha l+xml"));
  EXPECT_FALSE(IsXMLMimeType("aplicación/hal+xml"));
  EXPECT_FALSE(IsXMLMimeType("text/halé+xml"));
}

INSTANTIATE_TEST_SUITE_P(MimeUtilTest,
                         MimeUtilXmlMimeTypeTest,
                         testing::Bool());

TEST(MimeUtilTest, LookupTypes) {
  EXPECT_FALSE(IsUnsupportedTextMimeType("text/banana"));
  EXPECT_TRUE(IsUnsupportedTextMimeType("text/vcard"));

  EXPECT_TRUE(IsSupportedImageMimeType("image/jpeg"));
  EXPECT_TRUE(IsSupportedImageMimeType("Image/JPEG"));
  EXPECT_EQ(IsSupportedImageMimeType("image/avif"),
            BUILDFLAG(ENABLE_DAV1D_DECODER));
  EXPECT_FALSE(IsSupportedImageMimeType("image/lolcat"));
  EXPECT_FALSE(IsSupportedImageMimeType("Image/LolCat"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("text/html"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("text/css"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("text/"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("text/banana"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("Text/Banana"));
  EXPECT_FALSE(IsSupportedNonImageMimeType("text/vcard"));
  EXPECT_FALSE(IsSupportedNonImageMimeType("application/virus"));
  EXPECT_FALSE(IsSupportedNonImageMimeType("Application/VIRUS"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("application/json"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("application/+json"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("application/x-suggestions+json"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("application/x-s+json;x=2"));
#if BUILDFLAG(IS_ANDROID)
#if 0  // Disabled until http://crbug.com/318217 is resolved.
  EXPECT_TRUE(IsSupportedMediaMimeType("application/vnd.apple.mpegurl"));
  EXPECT_TRUE(IsSupportedMediaMimeType("application/x-mpegurl"));
  EXPECT_TRUE(IsSupportedMediaMimeType("Application/X-MPEGURL"));
#endif
#endif

  EXPECT_TRUE(IsSupportedMimeType("image/jpeg"));
  EXPECT_FALSE(IsSupportedMimeType("image/lolcat"));
  EXPECT_FALSE(IsSupportedMimeType("Image/LOLCAT"));
  EXPECT_TRUE(IsSupportedMimeType("text/html"));
  EXPECT_TRUE(IsSupportedMimeType("text/banana"));
  EXPECT_TRUE(IsSupportedMimeType("Text/BANANA"));
  EXPECT_FALSE(IsSupportedMimeType("text/vcard"));
  EXPECT_FALSE(IsSupportedMimeType("application/virus"));
  EXPECT_FALSE(IsSupportedMimeType("application/x-json"));
  EXPECT_FALSE(IsSupportedMimeType("Application/X-JSON"));
  EXPECT_FALSE(IsSupportedNonImageMimeType("application/vnd.doc;x=y+json"));
  EXPECT_FALSE(IsSupportedNonImageMimeType("Application/VND.DOC;X=Y+JSON"));
}

#if BUILDFLAG(ENABLE_JXL_DECODER)
class JxlFeatureFlagTest : public testing::TestWithParam<bool> {};

TEST_P(JxlFeatureFlagTest, JxlSupportMatchesFeatureFlag) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(blink::features::kJXLImageFormat, GetParam());
  EXPECT_EQ(IsSupportedImageMimeType("image/jxl"), GetParam());
}

INSTANTIATE_TEST_SUITE_P(MimeUtilTest, JxlFeatureFlagTest, testing::Bool());
#else
TEST(MimeUtilTest, JxlNotSupportedWhenDecoderDisabled) {
  EXPECT_FALSE(IsSupportedImageMimeType("image/jxl"));
}
#endif

}  // namespace blink
