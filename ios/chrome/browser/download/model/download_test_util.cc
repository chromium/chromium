// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/model/download_test_util.h"

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/path_service.h"

namespace testing {

const char kCalendarFilePath[] =
    "ios/testing/data/http_server_files/sample.ics";
const char kMobileConfigFilePath[] =
    "ios/testing/data/http_server_files/sample.mobileconfig";
const char kPkPassFilePath[] =
    "ios/testing/data/http_server_files/generic.pkpass";
const char kBundledPkPassFilePath[] =
    "ios/testing/data/http_server_files/bundle.pkpasses";
const char kSemiValidBundledPkPassFilePath[] =
    "ios/testing/data/http_server_files/semi_bundle.pkpasses";
const char kUsdzFilePath[] = "ios/testing/data/http_server_files/redchair.usdz";
const char kVcardFilePath[] = "ios/testing/data/http_server_files/vcard.vcf";

std::string GetTestFileContents(const char* file_path) {
  base::FilePath path;
  base::PathService::Get(base::DIR_ASSETS, &path);
  path = path.Append(FILE_PATH_LITERAL(file_path));
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  size_t size_to_read = file.GetLength();
  std::string contents;
  contents.resize(size_to_read);
  size_t size_read = file.ReadAtCurrentPos(&contents[0], size_to_read);
  contents.resize(size_read);
  return contents;
}

}  // namespace testing
