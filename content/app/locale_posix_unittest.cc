// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/locale_posix.h"

#include <locale.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

constexpr const char* kLocaleVariables[] = {
    "LANG",           "LC_ALL",
    "LC_CTYPE",       "LC_NUMERIC",
    "LC_TIME",        "LC_COLLATE",
    "LC_MONETARY",    "LC_MESSAGES",
    "LC_PAPER",       "LC_NAME",
    "LC_ADDRESS",     "LC_TELEPHONE",
    "LC_MEASUREMENT", "LC_IDENTIFICATION",
};

// A locale name that is never installed.
constexpr char kMissingLocale[] = "xx_ZZ.UTF-8";

class LocalePosixTest : public testing::Test {
 protected:
  void SetUp() override {
    env_ = base::Environment::Create();
    for (const char* var : kLocaleVariables) {
      std::string name(var);
      saved_env_.emplace_back(name, env_->GetVar(name));
      env_->UnSetVar(name);
    }
    saved_locale_ = setlocale(LC_ALL, nullptr);

    if (setlocale(LC_TIME, kMissingLocale)) {
      GTEST_SKIP() << kMissingLocale << " is unexpectedly installed";
    }
    if (!setlocale(LC_CTYPE, "C.UTF-8")) {
      GTEST_SKIP() << "C.UTF-8 is not available";
    }
    // Start from the state of a freshly started process.
    ASSERT_TRUE(setlocale(LC_ALL, "C"));
  }

  void TearDown() override {
    setlocale(LC_ALL, saved_locale_.c_str());
    for (const auto& [name, value] : saved_env_) {
      if (value) {
        env_->SetVar(name, *value);
      } else {
        env_->UnSetVar(name);
      }
    }
  }

  static void ExpectNonAsciiConversionWorks() {
    EXPECT_EQ(L"\u00e4\u00f6", base::SysNativeMBToWide("\xc3\xa4\xc3\xb6"));
    EXPECT_EQ(u"\u00e4\u00e4\u00e4.txt",
              base::FilePath("\xc3\xa4\xc3\xa4\xc3\xa4.txt").AsUTF16Unsafe());
    EXPECT_STREQ("C", setlocale(LC_NUMERIC, nullptr));
  }

  std::unique_ptr<base::Environment> env_;

 private:
  std::vector<std::pair<std::string, std::optional<std::string>>> saved_env_;
  std::string saved_locale_;
};

TEST_F(LocalePosixTest, ValidLocale) {
  env_->SetVar("LANG", "C.UTF-8");
  InitializeCLibraryLocale();
  ExpectNonAsciiConversionWorks();
}

// A single category naming a missing locale must not prevent the others from
// being set (https://crbug.com/40251852).
TEST_F(LocalePosixTest, OneCategoryMissing) {
  env_->SetVar("LANG", "C.UTF-8");
  env_->SetVar("LC_TIME", kMissingLocale);
  InitializeCLibraryLocale();
  ExpectNonAsciiConversionWorks();
}

TEST_F(LocalePosixTest, CtypeMissingFallsBackToUtf8) {
  env_->SetVar("LC_ALL", kMissingLocale);
  InitializeCLibraryLocale();
  ExpectNonAsciiConversionWorks();
}

// The "C" and "POSIX" locales use an ASCII character set, in which multibyte
// conversions fail on any non-ASCII file name (https://crbug.com/40719293).
TEST_F(LocalePosixTest, CLocaleUsesUtf8) {
  env_->SetVar("LANG", "C");
  InitializeCLibraryLocale();
  ExpectNonAsciiConversionWorks();
  EXPECT_STREQ("C", setlocale(LC_COLLATE, nullptr));
}

TEST_F(LocalePosixTest, PosixLocaleUsesUtf8) {
  env_->SetVar("LC_ALL", "POSIX");
  InitializeCLibraryLocale();
  ExpectNonAsciiConversionWorks();
}

TEST_F(LocalePosixTest, NoLocaleVariablesUsesUtf8) {
  InitializeCLibraryLocale();
  ExpectNonAsciiConversionWorks();
}

// A locale with a non-ASCII legacy character set is left alone.
TEST_F(LocalePosixTest, LegacyCharsetKept) {
  constexpr char kLatin9Locale[] = "it_IT@euro";
  if (!setlocale(LC_CTYPE, kLatin9Locale)) {
    GTEST_SKIP() << kLatin9Locale << " is not installed";
  }
  ASSERT_TRUE(setlocale(LC_ALL, "C"));
  env_->SetVar("LANG", kLatin9Locale);
  InitializeCLibraryLocale();
  EXPECT_STREQ(kLatin9Locale, setlocale(LC_CTYPE, nullptr));
}

}  // namespace

}  // namespace content
