// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_
#define EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "extensions/browser/api/document_scan/document_scan_interface.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/document_scan.h"

namespace extensions {
namespace api {

class DocumentScanScanFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("documentScan.scan", DOCUMENT_SCAN_SCAN)
  DocumentScanScanFunction();

 protected:
  ~DocumentScanScanFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  friend class DocumentScanScanFunctionTest;

  void OnScannerListReceived(
      const std::vector<DocumentScanInterface::ScannerDescription>&
          scanner_descriptions,
      const std::string& error);
  void OnResultsReceived(const std::string& scanned_image,
                         const std::string& mime_type,
                         const std::string& error);

  std::unique_ptr<document_scan::Scan::Params> params_;
  std::unique_ptr<DocumentScanInterface> document_scan_interface_;

  DISALLOW_COPY_AND_ASSIGN(DocumentScanScanFunction);
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_
