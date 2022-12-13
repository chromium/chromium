// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_data_source_fuchsia.h"

#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

namespace content {
namespace {

constexpr struct RobotoFontsInfo {
  int weight;
  const char* name;
} kRobotoFontsInfo[] = {
    {
        400,
        "Roboto",
    },
    {
        300,
        "Roboto Light",
    },
    {
        500,
        "Roboto Medium",
    },
};

}  // namespace

FontEnumerationDataSourceFuchsia::FontEnumerationDataSourceFuchsia() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FontEnumerationDataSourceFuchsia::~FontEnumerationDataSourceFuchsia() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

blink::FontEnumerationTable FontEnumerationDataSourceFuchsia::GetFonts(
    const std::string& locale) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  //  Use a FIDL interface when available.
  NOTIMPLEMENTED_LOG_ONCE();

  blink::FontEnumerationTable font_enumeration_table;

  // Add material icons
  blink::FontEnumerationTable_FontData* material_data =
      font_enumeration_table.add_fonts();
  material_data->set_postscript_name("Material Icons");
  material_data->set_full_name("Material Icons");
  material_data->set_family("Material Icons");
  material_data->set_style("Normal");

  // Add Roboto
  for (const auto& roboto_info : kRobotoFontsInfo) {
    blink::FontEnumerationTable_FontData* roboto_data =
        font_enumeration_table.add_fonts();
    roboto_data->set_postscript_name(roboto_info.name);
    roboto_data->set_full_name(roboto_info.name);
    roboto_data->set_family("sans-serif");
    roboto_data->set_style("Normal");
  }

  return font_enumeration_table;
}

}  // namespace content
