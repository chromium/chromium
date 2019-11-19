// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/document_scan/document_scan_api.h"

#include "base/bind.h"
#include "base/stl_util.h"

namespace {

const char kScannerNotAvailable[] = "Scanner not available";
const char kUserGestureRequiredError[] =
    "User gesture required to perform scan";

}  // namespace

namespace extensions {
namespace api {

DocumentScanScanFunction::DocumentScanScanFunction()
    : document_scan_interface_(DocumentScanInterface::CreateInstance()) {}

DocumentScanScanFunction::~DocumentScanScanFunction() {}

ExtensionFunction::ResponseAction DocumentScanScanFunction::Run() {
  params_ = document_scan::Scan::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  if (!user_gesture())
    return RespondNow(Error(kUserGestureRequiredError));

  document_scan_interface_->ListScanners(
      base::BindOnce(&DocumentScanScanFunction::OnScannerListReceived, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DocumentScanScanFunction::OnScannerListReceived(
    const std::vector<DocumentScanInterface::ScannerDescription>&
        scanner_descriptions,
    const std::string& error) {
  auto scanner_i = scanner_descriptions.cbegin();

  // If no |scanner_descriptions| is empty, this is an error.  If no
  // MIME types are specified, the first scanner is chosen.  If MIME
  // types are specified, the first scanner that supports one of these
  // MIME types is selected.
  if (params_->options.mime_types) {
    std::vector<std::string>& mime_types = *params_->options.mime_types;
    for (; scanner_i != scanner_descriptions.end(); ++scanner_i) {
      if (base::Contains(mime_types, scanner_i->image_mime_type)) {
        break;
      }
    }
  }

  if (scanner_i == scanner_descriptions.end()) {
    Respond(Error(kScannerNotAvailable));
    return;
  }

  // TODO(pstew): Call a delegate method here to select a scanner and options.
  document_scan_interface_->Scan(
      scanner_i->name, DocumentScanInterface::kScanModeColor, 0,
      base::BindOnce(&DocumentScanScanFunction::OnResultsReceived, this));
}

void DocumentScanScanFunction::OnResultsReceived(
    const std::string& scanned_image,
    const std::string& mime_type,
    const std::string& error) {
  // TODO(pstew): Enlist a delegate to display received scan in the UI
  //  and confirm that this scan should be sent to the caller.  If this
  // is a multi-page scan, provide a means for adding additional scanned
  // images up to the requested limit.

  if (!error.empty()) {
    Respond(Error(error));
    return;
  }

  document_scan::ScanResults scan_results;
  if (!scanned_image.empty()) {
    scan_results.data_urls.push_back(scanned_image);
  }
  scan_results.mime_type = mime_type;
  Respond(ArgumentList(document_scan::Scan::Results::Create(scan_results)));
}

}  // namespace api
}  // namespace extensions
