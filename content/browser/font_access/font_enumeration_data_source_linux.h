// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_LINUX_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_LINUX_H_

#include <string>

#include "base/sequence_checker.h"
#include "content/browser/font_access/font_enumeration_data_source.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

namespace content {

// ChromeOS and Linux implementation of FontEnumerationDataSource.
class FontEnumerationDataSourceLinux : public FontEnumerationDataSource {
 public:
  FontEnumerationDataSourceLinux();
  FontEnumerationDataSourceLinux(const FontEnumerationDataSourceLinux&) =
      delete;
  FontEnumerationDataSourceLinux& operator=(
      const FontEnumerationDataSourceLinux&) = delete;

  ~FontEnumerationDataSourceLinux() override;

  // FontEnumerationDataSource:
  blink::FontEnumerationTable GetFonts(const std::string& locale) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_LINUX_H_
