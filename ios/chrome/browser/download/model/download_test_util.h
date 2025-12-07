// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_TEST_UTIL_H_

#include <string>

#include "base/files/file_path.h"

namespace testing {

inline constexpr base::FilePath::StringViewType kCalendarFilePath =
    "ios/testing/data/http_server_files/sample.ics";
inline constexpr base::FilePath::StringViewType kMobileConfigFilePath =
    "ios/testing/data/http_server_files/sample.mobileconfig";
inline constexpr base::FilePath::StringViewType kAppleWalletOrderFilePath =
    "ios/testing/data/http_server_files/sample.order";
inline constexpr base::FilePath::StringViewType kPkPassFilePath =
    "ios/testing/data/http_server_files/generic.pkpass";
inline constexpr base::FilePath::StringViewType kBundledPkPassFilePath =
    "ios/testing/data/http_server_files/bundle.pkpasses";
inline constexpr base::FilePath::StringViewType
    kSemiValidBundledPkPassFilePath =
        "ios/testing/data/http_server_files/semi_bundle.pkpasses";
inline constexpr base::FilePath::StringViewType kUsdzFilePath =
    "ios/testing/data/http_server_files/redchair.usdz";
inline constexpr base::FilePath::StringViewType kVcardFilePath =
    "ios/testing/data/http_server_files/vcard.vcf";

// Returns contents of the test file at the given relative path in the chrome
// test directory.
std::string GetTestFileContents(base::FilePath::StringViewType file_path);

}  // namespace testing

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_TEST_UTIL_H_
