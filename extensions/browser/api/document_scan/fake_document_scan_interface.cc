// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/document_scan/fake_document_scan_interface.h"

#include <utility>

namespace extensions {
namespace api {

FakeDocumentScanInterface::FakeDocumentScanInterface() = default;
FakeDocumentScanInterface::~FakeDocumentScanInterface() = default;

void FakeDocumentScanInterface::SetListScannersResult(
    const std::vector<ScannerDescription>& scanner_descriptions,
    const std::string& error) {
  scanner_descriptions_ = scanner_descriptions;
  error_ = error;
}

void FakeDocumentScanInterface::SetScanResult(const std::string& scanned_image,
                                              const std::string& mime_type,
                                              const std::string& error) {
  scanned_image_ = scanned_image;
  mime_type_ = mime_type;
  error_ = error;
}

void FakeDocumentScanInterface::ListScanners(
    ListScannersResultsCallback callback) {
  std::move(callback).Run(scanner_descriptions_, error_);
}

void FakeDocumentScanInterface::Scan(const std::string& scanner_name,
                                     ScanMode mode,
                                     int resolution_dpi,
                                     ScanResultsCallback callback) {
  std::move(callback).Run(scanned_image_, mime_type_, error_);
}

}  // namespace api
}  // namespace extensions
