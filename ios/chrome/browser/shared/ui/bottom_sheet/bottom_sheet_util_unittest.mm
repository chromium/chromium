// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/bottom_sheet/bottom_sheet_util.h"

#import <string>
#import <string_view>

#import "base/i18n/rtl.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

// Web URLs whose path, query and ref are not displayed.
constexpr char kHttpsUrl[] = "https://example.com/path?query#ref";
constexpr char kHttpUrl[] = "http://example.com/path?query#ref";
constexpr char16_t kExampleHost[] = u"example.com";

constexpr char kTrivialWwwUrl[] = "https://www.example.com/path";

// "www." is part of the registrable domain of "www.com".
constexpr char kRegistrableWwwUrl[] = "https://www.com/path";
constexpr char16_t kRegistrableWwwHost[] = u"www.com";

// "www.intranet" has no registrable domain.
constexpr char kIntranetWwwUrl[] = "https://www.intranet/path";
constexpr char16_t kIntranetWwwHost[] = u"www.intranet";

constexpr char kHttpsDefaultPortUrl[] = "https://example.com:443/path";
constexpr char kHttpDefaultPortUrl[] = "http://example.com:80/path";

constexpr char kCustomPortUrl[] = "https://example.com:8443/path";
constexpr char kTrivialWwwCustomPortUrl[] = "https://www.example.com:8443/path";
constexpr char16_t kExampleHostWithCustomPort[] = u"example.com:8443";

// "äpple.de", a non-RTL IDN that passes the IDN spoof checks.
constexpr char kNonRtlIdnUrl[] = "https://xn--pple-koa.de/path";
constexpr char16_t kNonRtlIdnUnicodeHost[] = u"\u00e4pple.de";

// "äpple.com", a non-RTL IDN that looks like "apple.com".
constexpr char kLookalikeIdnUrl[] = "https://xn--pple-koa.com/path";
constexpr char16_t kLookalikeIdnPunycodeHost[] = u"xn--pple-koa.com";

// An LTR "paypal.com." prefix followed by Arabic (RTL) labels.
constexpr char kRtlIdnUrl[] = "https://paypal.com.xn--4gbrim.xn--ngbc5azd/path";
constexpr char16_t kRtlIdnPunycodeHost[] =
    u"paypal.com.xn--4gbrim.xn--ngbc5azd";

// Invalid because the port is out of range.
constexpr char kInvalidTrivialWwwUrl[] = "https://www.example.com:9999999";
constexpr char16_t kInvalidTrivialWwwUrlDisplay[] =
    u"https://www.example.com:9999999";

// Returns `text` enclosed in LTR embedding and pop directional formatting
// marks.
std::u16string LtrWrapped(std::u16string_view text) {
  std::u16string wrapped(1, base::i18n::kLeftToRightEmbeddingMark);
  wrapped.append(text);
  wrapped.push_back(base::i18n::kPopDirectionalFormatting);
  return wrapped;
}

using BottomSheetUtilTest = PlatformTest;

// Tests that only the host of an HTTP(S) URL is displayed.
TEST_F(BottomSheetUtilTest, OmitsWebSchemePathQueryAndRef) {
  EXPECT_EQ(LtrWrapped(kExampleHost),
            FormatUrlForBottomSheetDisplay(GURL(kHttpsUrl)));
  EXPECT_EQ(LtrWrapped(kExampleHost),
            FormatUrlForBottomSheetDisplay(GURL(kHttpUrl)));
}

// Tests that a trivial "www." subdomain is omitted.
TEST_F(BottomSheetUtilTest, OmitsTrivialWww) {
  EXPECT_EQ(LtrWrapped(kExampleHost),
            FormatUrlForBottomSheetDisplay(GURL(kTrivialWwwUrl)));
}

// Tests that "www." is kept when it is part of the registrable domain or when
// the host has no registrable domain.
TEST_F(BottomSheetUtilTest, KeepsNonTrivialWww) {
  EXPECT_EQ(LtrWrapped(kRegistrableWwwHost),
            FormatUrlForBottomSheetDisplay(GURL(kRegistrableWwwUrl)));
  EXPECT_EQ(LtrWrapped(kIntranetWwwHost),
            FormatUrlForBottomSheetDisplay(GURL(kIntranetWwwUrl)));
}

// Tests that the default port of the scheme is omitted.
TEST_F(BottomSheetUtilTest, OmitsDefaultPort) {
  EXPECT_EQ(LtrWrapped(kExampleHost),
            FormatUrlForBottomSheetDisplay(GURL(kHttpsDefaultPortUrl)));
  EXPECT_EQ(LtrWrapped(kExampleHost),
            FormatUrlForBottomSheetDisplay(GURL(kHttpDefaultPortUrl)));
}

// Tests that a non-default port is displayed, including when a trivial "www."
// subdomain is omitted.
TEST_F(BottomSheetUtilTest, ShowsNonDefaultPort) {
  EXPECT_EQ(LtrWrapped(kExampleHostWithCustomPort),
            FormatUrlForBottomSheetDisplay(GURL(kCustomPortUrl)));
  EXPECT_EQ(LtrWrapped(kExampleHostWithCustomPort),
            FormatUrlForBottomSheetDisplay(GURL(kTrivialWwwCustomPortUrl)));
}

// Tests that a non-RTL IDN is displayed in Unicode.
TEST_F(BottomSheetUtilTest, ShowsNonRtlIdnInUnicode) {
  EXPECT_EQ(LtrWrapped(kNonRtlIdnUnicodeHost),
            FormatUrlForBottomSheetDisplay(GURL(kNonRtlIdnUrl)));
}

// Tests that a non-RTL IDN that looks like another domain is displayed as
// punycode.
TEST_F(BottomSheetUtilTest, ShowsLookalikeIdnAsPunycode) {
  EXPECT_EQ(LtrWrapped(kLookalikeIdnPunycodeHost),
            FormatUrlForBottomSheetDisplay(GURL(kLookalikeIdnUrl)));
}

// Tests that an IDN containing strong RTL characters is displayed as punycode,
// so that its labels cannot be visually reordered.
TEST_F(BottomSheetUtilTest, ShowsRtlIdnAsPunycode) {
  EXPECT_EQ(LtrWrapped(kRtlIdnPunycodeHost),
            FormatUrlForBottomSheetDisplay(GURL(kRtlIdnUrl)));
}

// Tests that an invalid URL whose host starts with a trivial "www." is
// displayed through the formatter's fallback instead of being emptied.
TEST_F(BottomSheetUtilTest, ShowsInvalidUrlWithTrivialWww) {
  EXPECT_EQ(LtrWrapped(kInvalidTrivialWwwUrlDisplay),
            FormatUrlForBottomSheetDisplay(GURL(kInvalidTrivialWwwUrl)));
}

}  // namespace
