// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/font_access/font_enumeration_data_source_linux.h"

#include <fontconfig/fontconfig.h>

#include <memory>

#include "base/check_op.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

namespace content {

namespace {

std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)> CreateFormatPattern(
    const char* format) {
  std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)> pattern(
      FcPatternCreate(), FcPatternDestroy);
  FcPatternAddBool(pattern.get(), FC_SCALABLE, FcTrue);
  FcPatternAddString(pattern.get(), FC_FONTFORMAT,
                     reinterpret_cast<const FcChar8*>(format));
  return pattern;
}

// Returns a font set comprising of fonts in the provided object set.
FcFontSet* ListFonts(FcObjectSet* object_set) {
  FcFontSet* output = FcFontSetCreate();

  // See https://www.freetype.org/freetype2/docs/reference/ft2-font_formats.html
  // for the list of possible formats.
  const char* allowed_formats[] = {"TrueType", "CFF"};
  for (const auto* format : allowed_formats) {
    auto format_pattern = CreateFormatPattern(format);
    std::unique_ptr<FcFontSet, decltype(&FcFontSetDestroy)> fontset(
        FcFontList(nullptr, format_pattern.get(), object_set),
        FcFontSetDestroy);
    for (int j = 0; j < fontset->nfont; ++j) {
      FcPattern* font = fontset->fonts[j];
      // Increments the refcount for the font.
      FcPatternReference(font);
      FcBool result = FcFontSetAdd(output, font);
      DCHECK_EQ(result, FcTrue);
    }
  }
  return output;
}

}  // namespace

FontEnumerationDataSourceLinux::FontEnumerationDataSourceLinux() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FontEnumerationDataSourceLinux::~FontEnumerationDataSourceLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

blink::FontEnumerationTable FontEnumerationDataSourceLinux::GetFonts(
    const std::string& locale) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  blink::FontEnumerationTable font_enumeration_table;

  std::unique_ptr<FcObjectSet, decltype(&FcObjectSetDestroy)> object_set(
      FcObjectSetBuild(FC_POSTSCRIPT_NAME, FC_FULLNAME, FC_FAMILY, FC_STYLE,
                       nullptr),
      FcObjectSetDestroy);

  std::unique_ptr<FcFontSet, decltype(&FcFontSetDestroy)> fontset(
      ListFonts(object_set.get()), FcFontSetDestroy);

  // Used to filter duplicates.
  std::set<std::string> fonts_seen;

  for (int i = 0; i < fontset->nfont; ++i) {
    char* postscript_name = nullptr;
    if (FcPatternGetString(fontset->fonts[i], FC_POSTSCRIPT_NAME, 0,
                           reinterpret_cast<FcChar8**>(&postscript_name)) !=
        FcResultMatch) {
      // Skip incomplete or malformed font.
      continue;
    }

    char* full_name = nullptr;
    if (FcPatternGetString(fontset->fonts[i], FC_FULLNAME, 0,
                           reinterpret_cast<FcChar8**>(&full_name)) !=
        FcResultMatch) {
      // Skip incomplete or malformed font.
      continue;
    }

    char* family = nullptr;
    if (FcPatternGetString(fontset->fonts[i], FC_FAMILY, 0,
                           reinterpret_cast<FcChar8**>(&family)) !=
        FcResultMatch) {
      // Skip incomplete or malformed font.
      continue;
    }

    char* style = nullptr;
    if (FcPatternGetString(fontset->fonts[i], FC_STYLE, 0,
                           reinterpret_cast<FcChar8**>(&style)) !=
        FcResultMatch) {
      // Skip incomplete or malformed font.
      continue;
    }

    auto it_and_success = fonts_seen.emplace(postscript_name);
    if (!it_and_success.second) {
      // Skip duplicate.
      continue;
    }

    blink::FontEnumerationTable_FontData* data =
        font_enumeration_table.add_fonts();
    data->set_postscript_name(postscript_name);
    data->set_full_name(full_name);
    data->set_family(family);
    data->set_style(style);
  }

  return font_enumeration_table;
}

}  // namespace content
