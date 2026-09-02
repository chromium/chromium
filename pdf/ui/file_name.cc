// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ui/file_name.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

namespace chrome_pdf {

std::string GetFileNameForSaveFromUrlAndSuggestion(
    const std::string& url,
    const std::string& suggested_name) {
  // Generate a file name. Unfortunately, MIME type can't be provided, since it
  // requires IO.
  // Note that the content_disposition parameter is not used here because the
  // caller already parsed it and extracted out `suggested_name`.
  std::u16string file_name = net::GetSuggestedFilename(
      GURL(url), /*content_disposition=*/std::string(),
      /*referrer_charset=*/std::string(), /*suggested_name=*/suggested_name,
      /*mime_type=*/std::string(), /*default_name=*/std::string());
  return base::UTF16ToUTF8(file_name);
}

}  // namespace chrome_pdf
