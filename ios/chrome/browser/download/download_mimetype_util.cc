// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/download_mimetype_util.h"
#include "ios/chrome/browser/download/mime_type_util.h"

DownloadMimeTypeResult GetUmaResult(const std::string& mime_type) {
  if (mime_type == kPkPassMimeType)
    return DownloadMimeTypeResult::PkPass;

  if (mime_type == kZipArchiveMimeType)
    return DownloadMimeTypeResult::ZipArchive;

  if (mime_type == kMobileConfigurationType)
    return DownloadMimeTypeResult::iOSMobileConfig;

  if (mime_type == kMicrosoftApplicationMimeType)
    return DownloadMimeTypeResult::MicrosoftApplication;

  if (mime_type == kAndroidPackageArchiveMimeType)
    return DownloadMimeTypeResult::AndroidPackageArchive;

  if (mime_type == kVcardMimeType)
    return DownloadMimeTypeResult::VirtualContactFile;

  if (mime_type == kCalendarMimeType)
    return DownloadMimeTypeResult::Calendar;

  if (mime_type == kLegacyUsdzMimeType)
    return DownloadMimeTypeResult::LegacyUniversalSceneDescription;

  if (mime_type == kAppleDiskImageMimeType)
    return DownloadMimeTypeResult::AppleDiskImage;

  if (mime_type == kAppleInstallerPackageMimeType)
    return DownloadMimeTypeResult::AppleInstallerPackage;

  if (mime_type == kSevenZipArchiveMimeType)
    return DownloadMimeTypeResult::SevenZipArchive;

  if (mime_type == kRARArchiveMimeType)
    return DownloadMimeTypeResult::RARArchive;

  if (mime_type == kTarArchiveMimeType)
    return DownloadMimeTypeResult::TarArchive;

  if (mime_type == kAdobeFlashMimeType)
    return DownloadMimeTypeResult::AdobeFlash;

  if (mime_type == kAmazonKindleBookMimeType)
    return DownloadMimeTypeResult::AmazonKindleBook;

  if (mime_type == kBinaryDataMimeType)
    return DownloadMimeTypeResult::BinaryData;

  if (mime_type == kBitTorrentMimeType)
    return DownloadMimeTypeResult::BitTorrent;

  if (mime_type == kJavaArchiveMimeType)
    return DownloadMimeTypeResult::JavaArchive;

  if (mime_type == kVcardMimeType)
    return DownloadMimeTypeResult::Vcard;

  if (mime_type == kLegacyPixarUsdzMimeType)
    return DownloadMimeTypeResult::LegacyPixarUniversalSceneDescription;

  if (mime_type == kUsdzMimeType)
    return DownloadMimeTypeResult::UniversalSceneDescription;

  return DownloadMimeTypeResult::Other;
}
