// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/macros.h"
#include "extensions/browser/api/document_scan/document_scan_interface.h"

namespace {

const char kScanFunctionNotImplementedError[] = "Scan function not implemented";

}  // namespace

namespace extensions {
namespace api {

class DocumentScanInterfaceImpl : public DocumentScanInterface {
 public:
  DocumentScanInterfaceImpl() {}
  ~DocumentScanInterfaceImpl() override {}

  void ListScanners(ListScannersResultsCallback callback) override {
    std::move(callback).Run(std::vector<ScannerDescription>(), "");
  }
  void Scan(const std::string& scanner_name,
            ScanMode mode,
            int resolution_dpi,
            ScanResultsCallback callback) override {
    std::move(callback).Run("", "", kScanFunctionNotImplementedError);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DocumentScanInterfaceImpl);
};

// static
DocumentScanInterface* DocumentScanInterface::CreateInstance() {
  return new DocumentScanInterfaceImpl();
}

}  // namespace api
}  // namespace extensions
