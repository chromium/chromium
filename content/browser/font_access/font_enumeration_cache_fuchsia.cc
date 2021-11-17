// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_cache_fuchsia.h"

#include "base/notreached.h"

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

base::SequenceBound<FontEnumerationCache>
FontEnumerationCache::CreateForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    absl::optional<std::string> locale_override) {
  return base::SequenceBound<FontEnumerationCacheFuchsia>(
      std::move(task_runner), std::move(locale_override),
      base::PassKey<FontEnumerationCache>());
}

FontEnumerationCacheFuchsia::FontEnumerationCacheFuchsia(
    absl::optional<std::string> locale_override,
    base::PassKey<FontEnumerationCache>)
    : FontEnumerationCache(std::move(locale_override)) {}

FontEnumerationCacheFuchsia::~FontEnumerationCacheFuchsia() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

blink::FontEnumerationTable
FontEnumerationCacheFuchsia::ComputeFontEnumerationData(
    const std::string& locale) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTIMPLEMENTED_LOG_ONCE() << "Use a FIDL interface when available";

  blink::FontEnumerationTable font_enumeration_table;

  // Add material icons
  blink::FontEnumerationTable_FontMetadata* material_metadata =
      font_enumeration_table.add_fonts();
  material_metadata->set_postscript_name("Material Icons");
  material_metadata->set_full_name("Material Icons");
  material_metadata->set_family("Material Icons");
  material_metadata->set_style("Normal");
  material_metadata->set_italic(false);
  material_metadata->set_weight(400);
  material_metadata->set_stretch(1.0);

  // Add Roboto
  for (const auto& roboto_info : kRobotoFontsInfo) {
    blink::FontEnumerationTable_FontMetadata* roboto_metadata =
        font_enumeration_table.add_fonts();
    roboto_metadata->set_postscript_name(roboto_info.name);
    roboto_metadata->set_full_name(roboto_info.name);
    roboto_metadata->set_family("sans-serif");
    roboto_metadata->set_style("Normal");
    roboto_metadata->set_italic(false);
    roboto_metadata->set_weight(roboto_info.weight);
    roboto_metadata->set_stretch(1.0);
  }

  return font_enumeration_table;
}

}  // namespace content
