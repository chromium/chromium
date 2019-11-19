// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_FAKE_DOCUMENT_SCAN_INTERFACE_H_
#define EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_FAKE_DOCUMENT_SCAN_INTERFACE_H_

#include <string>

#include "extensions/browser/api/document_scan/document_scan_interface.h"

namespace extensions {
namespace api {

class FakeDocumentScanInterface : public DocumentScanInterface {
 public:
  FakeDocumentScanInterface();
  ~FakeDocumentScanInterface() override;

  void SetListScannersResult(
      const std::vector<ScannerDescription>& scanner_descriptions,
      const std::string& error);

  void SetScanResult(const std::string& scanned_image,
                     const std::string& mime_type,
                     const std::string& error);

  // DocumentScanInterface:
  void ListScanners(ListScannersResultsCallback callback) override;
  void Scan(const std::string& scanner_name,
            ScanMode mode,
            int resolution_dpi,
            ScanResultsCallback callback) override;

 private:
  std::vector<ScannerDescription> scanner_descriptions_;
  std::string scanned_image_;
  std::string mime_type_;
  std::string error_;
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_FAKE_DOCUMENT_SCAN_INTERFACE_H_
