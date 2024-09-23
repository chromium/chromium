// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <fontconfig/fontconfig.h>

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/values.h"
#include "content/common/font_list.h"

namespace content {

std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)> CreateFormatPattern(
    const char* format) {
  std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)> pattern(
      FcPatternCreate(), FcPatternDestroy);
  FcPatternAddBool(pattern.get(), FC_SCALABLE, FcTrue);
  FcPatternAddString(pattern.get(), FC_FONTFORMAT,
                     reinterpret_cast<const FcChar8*>(format));
  return pattern;
}

base::Value::List GetFontList_SlowBlocking() {
  DCHECK(GetFontListTaskRunner()->RunsTasksInCurrentSequence());

  base::Value::List font_list;

  std::unique_ptr<FcObjectSet, decltype(&FcObjectSetDestroy)> object_set(
      FcObjectSetBuild(FC_FAMILY, NULL), FcObjectSetDestroy);

  std::set<std::string> sorted_families;

  // See https://www.freetype.org/freetype2/docs/reference/ft2-font_formats.html
  // for the list of possible formats.
  for (const char* allowed_format : {"TrueType", "CFF"}) {
    auto format_pattern = CreateFormatPattern(allowed_format);
    std::unique_ptr<FcFontSet, decltype(&FcFontSetDestroy)> fontset(
        FcFontList(nullptr, format_pattern.get(), object_set.get()),
        FcFontSetDestroy);
    for (int j = 0; j < fontset->nfont; ++j) {
      char* family_string;
      FcPatternGetString(fontset->fonts[j], FC_FAMILY, 0,
                         reinterpret_cast<FcChar8**>(&family_string));
      sorted_families.insert(family_string);
    }
  }

  // For backwards compatibility with the older pango implementation, add the
  // three Fontconfig aliases that pango added. Our linux default settings for
  // fixed-width was "Monospace". If we remove that, this entry is not found in
  // the list anymore, see also:
  // https://git.gnome.org/browse/pango/tree/pango/pangofc-fontmap.c?h=1.40.1#n1351
  sorted_families.insert("Monospace");
  sorted_families.insert("Sans");
  sorted_families.insert("Serif");

  for (const auto& family : sorted_families) {
    base::Value::List font_item;
    font_item.Append(family);
    font_item.Append(family);  // localized name.
    // TODO(yusukes): Support localized family names.
    font_list.Append(std::move(font_item));
  }

  return font_list;
}

}  // namespace content
