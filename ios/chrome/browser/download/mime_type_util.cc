// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/mime_type_util.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"

// Extensions.
char kUsdzFileExtension[] = ".usdz";
char kRealityFileExtension[] = ".reality";

// MIME types.
char kMobileConfigurationType[] = "application/x-apple-aspen-config";
char kPkPassMimeType[] = "application/vnd.apple.pkpass";
char kVcardMimeType[] = "text/vcard";
char kUsdzMimeType[] = "model/vnd.usdz+zip";
char kLegacyUsdzMimeType[] = "model/usd";
char kLegacyPixarUsdzMimeType[] = "model/vnd.pixar.usd";
char kZipArchiveMimeType[] = "application/zip";
char kMicrosoftApplicationMimeType[] = "application/x-msdownload";
char kAndroidPackageArchiveMimeType[] =
    "application/vnd.android.package-archive";
char kCalendarMimeType[] = "text/calendar";
char kAppleDiskImageMimeType[] = "application/x-apple-diskimage";
char kAppleInstallerPackageMimeType[] = "application/vnd.apple.installer+xml";
char kSevenZipArchiveMimeType[] = "application/x-7z-compressed";
char kRARArchiveMimeType[] = "application/x-rar-compressed";
char kTarArchiveMimeType[] = "application/x-tar";
char kAdobeFlashMimeType[] = "application/x-shockwave-flash";
char kAmazonKindleBookMimeType[] = "application/vnd.amazon.ebook";
char kBinaryDataMimeType[] = "application/octet-stream";
char kBitTorrentMimeType[] = "application/x-bittorrent";
char kJavaArchiveMimeType[] = "application/java-archive";

bool IsUsdzFileFormat(const std::string& mime_type,
                      const std::u16string& suggested_filename) {
  base::FilePath suggested_path =
      base::FilePath(base::UTF16ToUTF8(suggested_filename));
  return mime_type == kUsdzMimeType || mime_type == kLegacyUsdzMimeType ||
         mime_type == kLegacyPixarUsdzMimeType ||
         suggested_path.MatchesExtension(kUsdzFileExtension) ||
         suggested_path.MatchesExtension(kRealityFileExtension);
}
