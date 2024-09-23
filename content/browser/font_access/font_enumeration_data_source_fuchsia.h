// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_FUCHSIA_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_FUCHSIA_H_

#include <string>

#include "base/sequence_checker.h"
#include "content/browser/font_access/font_enumeration_data_source.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

namespace content {

// Fuchsia implementation of FontEnumerationDataSource.
// TODO(crbug.com/42050376): Use fuchsia.font API when it provides enumeration.
class FontEnumerationDataSourceFuchsia : public FontEnumerationDataSource {
 public:
  FontEnumerationDataSourceFuchsia();

  FontEnumerationDataSourceFuchsia(const FontEnumerationDataSourceFuchsia&) =
      delete;
  FontEnumerationDataSourceFuchsia& operator=(
      const FontEnumerationDataSourceFuchsia&) = delete;

  ~FontEnumerationDataSourceFuchsia() override;

  // FontEnumerationDataSource:
  blink::FontEnumerationTable GetFonts(const std::string& locale) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_FUCHSIA_H_
