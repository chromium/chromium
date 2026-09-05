// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/email_utils.h"

#include <string>
#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr size_t kDefaultMaxLength = 36;

constexpr std::u16string_view kShortEmail = u"collaborator@example.com";
constexpr std::u16string_view kExactLengthEmail =
    u"123456789012345678@example12345.com";
constexpr std::u16string_view kLongUserEmail =
    u"remote.assistance.collaborator.user@example.com";
constexpr std::u16string_view kLongDomainEmail =
    u"support@corporate.verification.security.example.com.untrusted.tld";
constexpr std::u16string_view kBothLongEmail =
    u"remote.assistance.collaborator.session@verification.support.fake.tld";
constexpr std::u16string_view kDomainSpoofEmail =
    u"operator@example.com.fake-service.subdomain.attacker.untrusted.tld";
constexpr std::u16string_view kNonEmailIdentity =
    u"remote_assistance_desktop_collaborator_session_id";
constexpr std::u16string_view kUnicodeEmail =
    u"user_🚀_test_🌟_extra_long_name@subdomain_测试_🚀_example.com";
constexpr std::u16string_view kSmallMaxLengthEmail = u"user@example.com";
constexpr std::u16string_view kSmallMaxLengthNonEmail = u"remote_collaborator";

}  // namespace

TEST(EmailUtilsTest, ShortEmailNotTruncated) {
  EXPECT_EQ(ElideEmail(kShortEmail, kDefaultMaxLength), kShortEmail);
}

TEST(EmailUtilsTest, ExactLengthNotTruncated) {
  ASSERT_EQ(kExactLengthEmail.length(), 35u);
  EXPECT_EQ(ElideEmail(kExactLengthEmail, 35), kExactLengthEmail);
}

TEST(EmailUtilsTest, LongUsernameShortDomainPreservesUsernamePrefixAndDomain) {
  // Username is 35 chars, domain is 11 chars. Total: 47 > 36.
  std::string elided =
      base::UTF16ToUTF8(ElideEmail(kLongUserEmail, kDefaultMaxLength));

  EXPECT_LE(base::UTF8ToUTF16(elided).length(), kDefaultMaxLength);
  // Domain is untouched.
  EXPECT_THAT(elided, testing::EndsWith("@example.com"));
  // Username prefix is preserved with tail ellipsis.
  EXPECT_THAT(elided, testing::StartsWith("remote.assistance"));
  EXPECT_THAT(elided, testing::HasSubstr("…@example.com"));
}

TEST(EmailUtilsTest, LongDomainShortUsernamePreservesDomainSuffix) {
  // Username is 7 chars, domain is 57 chars.
  std::string elided =
      base::UTF16ToUTF8(ElideEmail(kLongDomainEmail, kDefaultMaxLength));

  EXPECT_LE(base::UTF8ToUTF16(elided).length(), kDefaultMaxLength);
  // Username is untouched.
  EXPECT_THAT(elided, testing::StartsWith("support@"));
  // Authentic domain suffix is preserved, defeating spoofing.
  EXPECT_THAT(elided, testing::EndsWith("untrusted.tld"));
  EXPECT_THAT(elided, testing::HasSubstr("…"));
}

TEST(EmailUtilsTest, BothLongPreservesBothParts) {
  std::string elided =
      base::UTF16ToUTF8(ElideEmail(kBothLongEmail, kDefaultMaxLength));

  EXPECT_LE(base::UTF8ToUTF16(elided).length(), kDefaultMaxLength);
  // Preserves username head.
  EXPECT_THAT(elided, testing::StartsWith("remote.assist"));
  // Preserves authentic domain tail.
  EXPECT_THAT(elided, testing::EndsWith("fake.tld"));
  EXPECT_THAT(elided, testing::HasSubstr("@"));
}

TEST(EmailUtilsTest, DomainSpoofAttackPreservesTrueSuffix) {
  std::string elided =
      base::UTF16ToUTF8(ElideEmail(kDomainSpoofEmail, kDefaultMaxLength));

  EXPECT_LE(base::UTF8ToUTF16(elided).length(), kDefaultMaxLength);
  EXPECT_THAT(elided, testing::StartsWith("operator@"));
  EXPECT_THAT(elided, testing::EndsWith("untrusted.tld"));
}

TEST(EmailUtilsTest, NonEmailIdentityMiddleElided) {
  std::string elided =
      base::UTF16ToUTF8(ElideEmail(kNonEmailIdentity, kDefaultMaxLength));

  EXPECT_LE(base::UTF8ToUTF16(elided).length(), kDefaultMaxLength);
  // Both head and tail are preserved.
  EXPECT_THAT(elided, testing::StartsWith("remote_assistance"));
  EXPECT_THAT(elided, testing::EndsWith("session_id"));
  EXPECT_THAT(elided, testing::HasSubstr("…"));
}

TEST(EmailUtilsTest, UnicodeAndSurrogatePairsDoNotSplit) {
  // Includes emojis (surrogate pairs) and CJK.
  std::u16string elided = ElideEmail(kUnicodeEmail, kDefaultMaxLength);

  EXPECT_LE(elided.length(), kDefaultMaxLength);
  EXPECT_THAT(base::UTF16ToUTF8(elided), testing::EndsWith("example.com"));
  // Ensure the string is valid UTF-16 with no lone surrogates.
  for (size_t i = 0; i < elided.length(); ++i) {
    char16_t c = elided[i];
    if (c >= 0xD800 && c <= 0xDBFF) {
      // Lead surrogate must be followed by trail surrogate.
      ASSERT_LT(i + 1, elided.length());
      char16_t next = elided[i + 1];
      EXPECT_GE(next, 0xDC00);
      EXPECT_LE(next, 0xDFFF);
      ++i;
    } else {
      // Must not be an isolated trail surrogate.
      EXPECT_FALSE(c >= 0xDC00 && c <= 0xDFFF);
    }
  }
}

TEST(EmailUtilsTest, SmallMaxLength) {
  EXPECT_TRUE(ElideEmail(kSmallMaxLengthEmail, 0).empty());
  EXPECT_EQ(ElideEmail(kSmallMaxLengthEmail, 1), u"…");
  EXPECT_LE(ElideEmail(kSmallMaxLengthEmail, 2).length(), 2u);
  EXPECT_LE(ElideEmail(kSmallMaxLengthEmail, 3).length(), 3u);
  std::u16string elided_5 = ElideEmail(kSmallMaxLengthEmail, 5);
  EXPECT_LE(elided_5.length(), 5u);

  EXPECT_TRUE(ElideEmail(kSmallMaxLengthNonEmail, 0).empty());
  EXPECT_EQ(ElideEmail(kSmallMaxLengthNonEmail, 1), u"…");
  EXPECT_LE(ElideEmail(kSmallMaxLengthNonEmail, 2).length(), 2u);
}

}  // namespace remoting
