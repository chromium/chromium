/**
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/layout/forms/layout_text_control.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/text_utils.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

namespace layout_text_control {

void StyleDidChange(HTMLElement* inner_editor,
                    const ComputedStyle* old_style,
                    const ComputedStyle& new_style) {
  if (!inner_editor) {
    return;
  }
  LayoutBlock* inner_editor_layout_object =
      To<LayoutBlock>(inner_editor->GetLayoutObject());
  if (inner_editor_layout_object) {
    // TODO(https://crbug.com/1101564):
    // This is necessary to update the style on the inner_editor based on the
    // changes in the input element ComputedStyle.
    // (See TextControlInnerEditorElement::CreateInnerEditorStyle()).
    {
      StyleEngine::AllowMarkStyleDirtyFromRecalcScope scope(
          inner_editor->GetDocument().GetStyleEngine());
      inner_editor->SetNeedsStyleRecalc(
          kLocalStyleChange,
          StyleChangeReasonForTracing::Create(style_change_reason::kControl));
    }

    // The inner editor element uses the LayoutTextControl's ::selection style
    // (see: HighlightPseudoStyle in highlight_painting_utils.cc) so ensure
    // the inner editor selection is invalidated anytime style changes and a
    // ::selection style is or was present on LayoutTextControl.
    if (new_style.HasPseudoElementStyle(kPseudoIdSelection) ||
        (old_style && old_style->HasPseudoElementStyle(kPseudoIdSelection))) {
      inner_editor_layout_object->InvalidateSelectedChildrenOnStyleChange();
    }
  }
}

int ScrollbarThickness(const LayoutBox& box) {
  const Page& page = *box.GetDocument().GetPage();
  return page.GetScrollbarTheme().ScrollbarThickness(
      page.GetChromeClient().WindowToViewportScalar(box.GetFrame(), 1.0f),
      box.StyleRef().UsedScrollbarWidth());
}

void HitInnerEditorElement(const LayoutBox& box,
                           HTMLElement& inner_editor,
                           HitTestResult& result,
                           const HitTestLocation& hit_test_location,
                           const PhysicalOffset& accumulated_offset) {
  if (!inner_editor.GetLayoutObject()) {
    return;
  }

  PhysicalOffset local_point =
      hit_test_location.Point() - accumulated_offset -
      inner_editor.GetLayoutObject()->LocalToAncestorPoint(PhysicalOffset(),
                                                           &box);
  result.OverrideNodeAndPosition(&inner_editor, local_point);
}

static const char* const kFontFamiliesWithInvalidCharWidth[] = {
    "American Typewriter",
    "Arial Hebrew",
    "Chalkboard",
    "Cochin",
    "Corsiva Hebrew",
    "Courier",
    "Euphemia UCAS",
    "Geneva",
    "Gill Sans",
    "Hei",
    "Helvetica",
    "Hoefler Text",
    "InaiMathi",
    "Kai",
    "Lucida Grande",
    "Marker Felt",
    "Monaco",
    "Mshtakan",
    "New Peninim MT",
    "Osaka",
    "Raanana",
    "STHeiti",
    "Symbol",
    "Times",
    "Apple Braille",
    "Apple LiGothic",
    "Apple LiSung",
    "Apple Symbols",
    "AppleGothic",
    "AppleMyungjo",
    "#GungSeo",
    "#HeadLineA",
    "#PCMyungjo",
    "#PilGi",
};

// For font families where any of the fonts don't have a valid entry in the OS/2
// table for avgCharWidth, fallback to the legacy webkit behavior of getting the
// avgCharWidth from the width of a '0'. This only seems to apply to a fixed
// number of Mac fonts, but, in order to get similar rendering across platforms,
// we do this check for all platforms.
bool HasValidAvgCharWidth(const Font& font) {
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data) {
    return false;
  }
  // Some fonts match avgCharWidth to CJK full-width characters.
  // Heuristic check to avoid such fonts.
  const FontMetrics& metrics = font_data->GetFontMetrics();
  if (metrics.HasZeroWidth() &&
      font_data->AvgCharWidth() > metrics.ZeroWidth() * 1.7) {
    return false;
  }

  static HashSet<AtomicString>* font_families_with_invalid_char_width_map =
      nullptr;

  const AtomicString& family = font.GetFontDescription().Family().FamilyName();
  if (family.empty()) {
    return false;
  }

  if (!font_families_with_invalid_char_width_map) {
    font_families_with_invalid_char_width_map = new HashSet<AtomicString>;

    for (size_t i = 0; i < std::size(kFontFamiliesWithInvalidCharWidth); ++i) {
      font_families_with_invalid_char_width_map->insert(
          AtomicString(kFontFamiliesWithInvalidCharWidth[i]));
    }
  }

  return !font_families_with_invalid_char_width_map->Contains(family);
}

float GetAvgCharWidth(const ComputedStyle& style) {
  const Font& font = style.GetFont();
  const SimpleFontData* primary_font = font.PrimaryFont();
  if (primary_font && HasValidAvgCharWidth(font)) {
    const float width = primary_font->AvgCharWidth();
    // We apply roundf() only if the fractional part of |width| is >= 0.5
    // because:
    // * We have done it for a long time.
    // * Removing roundf() would make the intrinsic width smaller, and it
    //   would have a compatibility risk.
    return std::max(width, roundf(width));
  }

  const UChar kCh = '0';
  return ComputeTextWidth(StringView(&kCh, 1u), style);
}

}  // namespace layout_text_control

}  // namespace blink
