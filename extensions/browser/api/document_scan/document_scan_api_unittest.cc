// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/document_scan/document_scan_api.h"

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "extensions/browser/api/document_scan/fake_document_scan_interface.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/api_unittest.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace api {

// Tests of networking_private_crypto support for Networking Private API.
class DocumentScanScanFunctionTest : public ApiUnitTest {
 public:
  DocumentScanScanFunctionTest()
      : function_(base::MakeRefCounted<DocumentScanScanFunction>()),
        document_scan_interface_(new FakeDocumentScanInterface()) {}
  ~DocumentScanScanFunctionTest() override {}

  void SetUp() override {
    ApiUnitTest::SetUp();
    // Passes ownership.
    function_->document_scan_interface_.reset(document_scan_interface_);
  }

 protected:
  std::string RunFunctionAndReturnError(const std::string& args) {
    function_->set_extension(extension());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function_.get(), args, browser_context(), api_test_utils::NONE);
    return error;
  }

  scoped_refptr<DocumentScanScanFunction> function_;
  FakeDocumentScanInterface* document_scan_interface_;  // Owned by function_.
};

TEST_F(DocumentScanScanFunctionTest, GestureRequired) {
  EXPECT_EQ("User gesture required to perform scan",
            RunFunctionAndReturnError("[{}]"));
}

TEST_F(DocumentScanScanFunctionTest, NoScanners) {
  function_->set_user_gesture(true);
  document_scan_interface_->SetListScannersResult({}, "");
  EXPECT_EQ("Scanner not available", RunFunctionAndReturnError("[{}]"));
}

TEST_F(DocumentScanScanFunctionTest, NoMatchingScanners) {
  function_->set_user_gesture(true);
  std::vector<DocumentScanInterface::ScannerDescription> scanner_list;
  DocumentScanInterface::ScannerDescription scanner;
  scanner.image_mime_type = "img/fresco";
  scanner_list.push_back(scanner);
  document_scan_interface_->SetListScannersResult(scanner_list, "");
  EXPECT_EQ(
      "Scanner not available",
      RunFunctionAndReturnError("[{\"mimeTypes\": [\"img/silverpoint\"]}]"));
}

TEST_F(DocumentScanScanFunctionTest, ScanFailure) {
  function_->set_user_gesture(true);
  std::vector<DocumentScanInterface::ScannerDescription> scanner_list;
  DocumentScanInterface::ScannerDescription scanner;
  const char kMimeType[] = "img/tempera";
  const char kScannerName[] = "Michelangelo";
  scanner.name = kScannerName;
  scanner.image_mime_type = kMimeType;
  scanner_list.push_back(scanner);
  document_scan_interface_->SetListScannersResult(scanner_list, "");
  const char kScanError[] = "Someone ate all the eggs";
  document_scan_interface_->SetScanResult("", "", kScanError);
  EXPECT_EQ(kScanError,
            RunFunctionAndReturnError("[{\"mimeTypes\": [\"img/tempera\"]}]"));
}

TEST_F(DocumentScanScanFunctionTest, Success) {
  std::vector<DocumentScanInterface::ScannerDescription> scanner_list;
  scanner_list.push_back(DocumentScanInterface::ScannerDescription());
  document_scan_interface_->SetListScannersResult(scanner_list, "");
  const char kScanData[] = "A beautiful picture";
  const char kMimeType[] = "img/encaustic";
  document_scan_interface_->SetScanResult(kScanData, kMimeType, "");
  function_->set_user_gesture(true);
  std::unique_ptr<base::DictionaryValue> result(
      RunFunctionAndReturnDictionary(function_.get(), "[{}]"));
  ASSERT_NE(nullptr, result.get());
  document_scan::ScanResults scan_results;
  EXPECT_TRUE(document_scan::ScanResults::Populate(*result, &scan_results));
  EXPECT_THAT(scan_results.data_urls, testing::ElementsAre(kScanData));
  EXPECT_EQ(kMimeType, scan_results.mime_type);
}

}  // namespace api
}  // namespace extensions
