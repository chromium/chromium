// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/directory_listing.h"

#include "base/i18n/time_formatting.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "net/base/escape.h"
#include "net/base/net_module.h"
#include "net/grit/net_resources.h"

namespace net {

std::string GetDirectoryListingHeader(const base::string16& title) {
  scoped_refptr<base::RefCountedMemory> header(
      NetModule::GetResource(IDR_DIR_HEADER_HTML));
  // This can be null in unit tests.
  DLOG_IF(WARNING, !header) << "Missing resource: directory listing header";

  std::string result;
  if (header)
    result.assign(header->front_as<char>(), header->size());

  result.append("<script>start(");
  base::EscapeJSONString(title, true, &result);
  result.append(");</script>\n");

  return result;
}

std::string GetDirectoryListingEntry(const base::string16& name,
                                     const std::string& raw_bytes,
                                     bool is_dir,
                                     int64_t size,
                                     base::Time modified) {
  std::string result;
  result.append("<script>addRow(");
  base::EscapeJSONString(name, true, &result);
  result.append(",");
  if (raw_bytes.empty()) {
    base::EscapeJSONString(EscapePath(base::UTF16ToUTF8(name)), true, &result);
  } else {
    base::EscapeJSONString(EscapePath(raw_bytes), true, &result);
  }

  if (is_dir) {
    result.append(",1,");
  } else {
    result.append(",0,");
  }

  // Negative size means unknown or not applicable (e.g. directory).
  std::stringstream raw_size_string_stream;
  raw_size_string_stream << size << ",";
  result.append(raw_size_string_stream.str());

  base::string16 size_string;
  if (size >= 0)
    size_string = base::FormatBytesUnlocalized(size);
  base::EscapeJSONString(size_string, true, &result);

  result.append(",");

  // |modified| can be NULL in FTP listings.
  base::string16 modified_str;
  if (modified.is_null()) {
    result.append("0,");
  } else {
    std::stringstream raw_time_string_stream;
    // Certain access paths can only get up to seconds resolution, so here we
    // output the raw time value in seconds for consistency.
    raw_time_string_stream << modified.ToJavaTime() /
                                  base::Time::kMillisecondsPerSecond
                           << ",";
    result.append(raw_time_string_stream.str());

    modified_str = base::TimeFormatShortDateAndTime(modified);
  }

  base::EscapeJSONString(modified_str, true, &result);
  result.append(");</script>\n");

  return result;
}

std::string GetParentDirectoryLink() {
  return std::string("<script>onHasParentDirectory();</script>\n");
}

}  // namespace net
