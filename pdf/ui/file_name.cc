// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ui/file_name.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

namespace chrome_pdf {

std::string GetFileNameForSaveFromUrl(const std::string& url) {
  // Generate a file name. Unfortunately, MIME type can't be provided, since it
  // requires IO.
  std::u16string file_name = net::GetSuggestedFilename(
      GURL(url), /*content_disposition=*/std::string(),
      /*referrer_charset=*/std::string(), /*suggested_name=*/std::string(),
      /*mime_type=*/std::string(), /*default_name=*/std::string());
  return base::UTF16ToUTF8(file_name);
}

}  // namespace chrome_pdf
