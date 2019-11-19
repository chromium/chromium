// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/usdz_mime_type.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"

char kUsdzFileExtension[] = ".usdz";
char kUsdzMimeType[] = "model/vnd.usdz+zip";
char kLegacyUsdzMimeType[] = "model/usd";
char kLegacyPixarUsdzMimeType[] = "model/vnd.pixar.usd";

bool IsUsdzFileFormat(const std::string& mime_type,
                      const base::string16& suggested_filename) {
  return mime_type == kUsdzMimeType || mime_type == kLegacyUsdzMimeType ||
         mime_type == kLegacyPixarUsdzMimeType ||
         base::FilePath(base::UTF16ToUTF8(suggested_filename))
             .MatchesExtension(kUsdzFileExtension);
}
