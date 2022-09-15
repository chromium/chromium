// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_WIN_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_WIN_H_

#include <string>

#include "base/sequence_checker.h"
#include "content/browser/font_access/font_enumeration_data_source.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

namespace content {

// Windows implementation of FontEnumerationDataSource.
//
// Calls DirectWrite font APIs. Requires Windows 7 with KB2670838 and newer.
class FontEnumerationDataSourceWin : public FontEnumerationDataSource {
 public:
  FontEnumerationDataSourceWin();

  FontEnumerationDataSourceWin(const FontEnumerationDataSourceWin&) = delete;
  FontEnumerationDataSourceWin& operator=(const FontEnumerationDataSourceWin&) =
      delete;

  ~FontEnumerationDataSourceWin() override;

  // FontEnumerationDataSource:
  blink::FontEnumerationTable GetFonts(const std::string& locale) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_WIN_H_
