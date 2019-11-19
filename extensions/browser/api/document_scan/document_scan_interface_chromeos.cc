// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/document_scan/document_scan_interface_chromeos.h"

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/lorgnette_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

constexpr char kImageScanFailedError[] = "Image scan failed";
constexpr char kScannerImageMimeTypePng[] = "image/png";
constexpr char kPngImageDataUrlPrefix[] = "data:image/png;base64,";

chromeos::LorgnetteManagerClient* GetLorgnetteManagerClient() {
  DCHECK(chromeos::DBusThreadManager::IsInitialized());
  return chromeos::DBusThreadManager::Get()->GetLorgnetteManagerClient();
}

}  // namespace

namespace extensions {
namespace api {

DocumentScanInterfaceChromeos::DocumentScanInterfaceChromeos() = default;

DocumentScanInterfaceChromeos::~DocumentScanInterfaceChromeos() = default;

void DocumentScanInterfaceChromeos::ListScanners(
    ListScannersResultsCallback callback) {
  GetLorgnetteManagerClient()->ListScanners(
      base::BindOnce(&DocumentScanInterfaceChromeos::OnScannerListReceived,
                     base::Unretained(this), std::move(callback)));
}

void DocumentScanInterfaceChromeos::OnScannerListReceived(
    ListScannersResultsCallback callback,
    base::Optional<chromeos::LorgnetteManagerClient::ScannerTable> scanners) {
  std::vector<ScannerDescription> scanner_descriptions;
  if (scanners.has_value()) {
    for (const auto& scanner : scanners.value()) {
      ScannerDescription description;
      description.name = scanner.first;
      const auto& entry = scanner.second;
      auto info_it = entry.find(lorgnette::kScannerPropertyManufacturer);
      if (info_it != entry.end()) {
        description.manufacturer = info_it->second;
      }
      info_it = entry.find(lorgnette::kScannerPropertyModel);
      if (info_it != entry.end()) {
        description.model = info_it->second;
      }
      info_it = entry.find(lorgnette::kScannerPropertyType);
      if (info_it != entry.end()) {
        description.scanner_type = info_it->second;
      }
      description.image_mime_type = kScannerImageMimeTypePng;
      scanner_descriptions.push_back(description);
    }
  }
  const std::string kNoError;
  std::move(callback).Run(scanner_descriptions, kNoError);
}

void DocumentScanInterfaceChromeos::Scan(const std::string& scanner_name,
                                         ScanMode mode,
                                         int resolution_dpi,
                                         ScanResultsCallback callback) {
  VLOG(1) << "Choosing scanner " << scanner_name;
  chromeos::LorgnetteManagerClient::ScanProperties properties;
  switch (mode) {
    case kScanModeColor:
      properties.mode = lorgnette::kScanPropertyModeColor;
      break;

    case kScanModeGray:
      properties.mode = lorgnette::kScanPropertyModeGray;
      break;

    case kScanModeLineart:
      properties.mode = lorgnette::kScanPropertyModeLineart;
      break;
  }

  if (resolution_dpi != 0) {
    properties.resolution_dpi = resolution_dpi;
  }

  GetLorgnetteManagerClient()->ScanImageToString(
      scanner_name, properties,
      base::BindOnce(&DocumentScanInterfaceChromeos::OnScanCompleted,
                     base::Unretained(this), std::move(callback)));
}

void DocumentScanInterfaceChromeos::OnScanCompleted(
    ScanResultsCallback callback,
    base::Optional<std::string> image_data) {
  VLOG(1) << "ScanImage returns " << image_data.has_value();
  if (!image_data.has_value()) {
    std::move(callback).Run(std::string(), std::string(),
                            kImageScanFailedError);
    return;
  }
  std::string image_base64;
  base::Base64Encode(image_data.value(), &image_base64);
  std::move(callback).Run(kPngImageDataUrlPrefix + image_base64,
                          kScannerImageMimeTypePng, std::string() /* error */);
}

// static
DocumentScanInterface* DocumentScanInterface::CreateInstance() {
  return new DocumentScanInterfaceChromeos();
}

}  // namespace api
}  // namespace extensions
