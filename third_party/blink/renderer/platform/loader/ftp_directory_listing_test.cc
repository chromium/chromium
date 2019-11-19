// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/ftp_directory_listing.h"

#include <string>

#include "base/test/icu_test_util.h"
#include "net/net_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace blink {
namespace {

class ScopedRestoreDefaultTimezone {
  STACK_ALLOCATED();

 public:
  explicit ScopedRestoreDefaultTimezone(const char* zoneid) {
    original_zone_.reset(icu::TimeZone::createDefault());
    icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone(zoneid));
  }
  ~ScopedRestoreDefaultTimezone() {
    icu::TimeZone::adoptDefault(original_zone_.release());
  }

  ScopedRestoreDefaultTimezone(const ScopedRestoreDefaultTimezone&) = delete;
  ScopedRestoreDefaultTimezone& operator=(const ScopedRestoreDefaultTimezone&) =
      delete;

 private:
  std::unique_ptr<icu::TimeZone> original_zone_;
};

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
TEST(FtpDirectoryListingTest, Top) {
  base::test::ScopedRestoreICUDefaultLocale locale("en_US");
  ScopedRestoreDefaultTimezone timezone("Asia/Tokyo");

  const KURL url("ftp://ftp.example.com/");

  const std::string input = "drwxr-xr-x  1 ftp ftp 17 Feb 15 2016 top\r\n";
  // Referring to code in net/base/dir_header.html, but the code itself
  // is not included in the expectation due to unittest configuration.
  std::string expected = R"JS(<script>start("/");</script>
<script>addRow("top","top",1,0,"0 B",1455494400,"2/15/16, 9:00:00 AM");</script>
)JS";
  auto input_buffer = SharedBuffer::Create();
  input_buffer->Append(input.data(), input.size());

  auto output = GenerateFtpDirectoryListingHtml(url, input_buffer.get());
  std::string flatten_output;
  for (const auto span : *output) {
    flatten_output.append(span.data(), span.size());
  }

  EXPECT_EQ(expected, flatten_output);
}
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
TEST(FtpDirectoryListingTest, NonTop) {
  base::test::ScopedRestoreICUDefaultLocale locale("en_US");
  ScopedRestoreDefaultTimezone timezone("Asia/Tokyo");
  const KURL url("ftp://ftp.example.com/foo/");

  const std::string input = "drwxr-xr-x  1 ftp ftp 17 Feb 15 2016 dir\r\n";
  // Referring to code in net/base/dir_header.html, but the code itself
  // is not included in the expectation due to unittest configuration.
  std::string expected = R"JS(<script>start("/foo/");</script>
<script>onHasParentDirectory();</script>
<script>addRow("dir","dir",1,0,"0 B",1455494400,"2/15/16, 9:00:00 AM");</script>
)JS";

  auto input_buffer = SharedBuffer::Create();
  input_buffer->Append(input.data(), input.size());

  auto output = GenerateFtpDirectoryListingHtml(url, input_buffer.get());
  std::string flatten_output;
  for (const auto span : *output) {
    flatten_output.append(span.data(), span.size());
  }

  EXPECT_EQ(expected, flatten_output);
}
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

TEST(FtpDirectoryListingTest, Fail) {
  base::test::ScopedRestoreICUDefaultLocale locale("en_US");
  ScopedRestoreDefaultTimezone timezone("Asia/Tokyo");
  const KURL url("ftp://ftp.example.com/");
  auto input = SharedBuffer::Create();
  input->Append("bogus", 5u);
  std::string expected = R"JS(<script>start("/");</script>
<script>onListingParsingError();</script>
)JS";
  auto output = GenerateFtpDirectoryListingHtml(url, input.get());
  std::string flatten_output;
  for (const auto span : *output) {
    flatten_output.append(span.data(), span.size());
  }

  EXPECT_EQ(expected, flatten_output);
}

}  // namespace

}  // namespace blink
