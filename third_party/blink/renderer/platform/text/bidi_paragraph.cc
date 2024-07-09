// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/text//bidi_paragraph.h"

#include "third_party/blink/renderer/platform/text/icu_error.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

bool BidiParagraph::SetParagraph(const String& text,
                                 std::optional<TextDirection> base_direction) {
  DCHECK(!text.IsNull());
  DCHECK(!ubidi_);
  ubidi_ = UBidiPtr(ubidi_open());

  UBiDiLevel para_level;
  if (base_direction) {
    base_direction_ = *base_direction;
    para_level = IsLtr(base_direction_) ? UBIDI_LTR : UBIDI_RTL;
  } else {
    para_level = UBIDI_DEFAULT_LTR;
  }

  ICUError error;
  ubidi_setPara(ubidi_.get(), text.Characters16(), text.length(), para_level,
                nullptr, &error);
  if (U_FAILURE(error)) {
    NOTREACHED_IN_MIGRATION();
    ubidi_ = nullptr;
    return false;
  }

  if (!base_direction) {
    base_direction_ = DirectionFromLevel(ubidi_getParaLevel(ubidi_.get()));
  }

  return true;
}

// static
template <>
std::optional<TextDirection> BidiParagraph::BaseDirectionForString(
    base::span<const LChar> text,
    bool (*stop_at)(UChar)) {
  for (const LChar ch : text) {
    if (u_charDirection(ch) == U_LEFT_TO_RIGHT) {
      return TextDirection::kLtr;
    }

    if (stop_at && stop_at(ch)) {
      break;
    }
  }
  return std::nullopt;
}

// static
template <>
std::optional<TextDirection> BidiParagraph::BaseDirectionForString(
    base::span<const UChar> text,
    bool (*stop_at)(UChar)) {
  const UChar* data = text.data();
  const size_t len = text.size();
  for (size_t i = 0; i < len;) {
    UChar32 ch;
    U16_NEXT(data, i, len, ch);
    switch (u_charDirection(ch)) {
      case U_LEFT_TO_RIGHT:
        return TextDirection::kLtr;
      case U_RIGHT_TO_LEFT:
      case U_RIGHT_TO_LEFT_ARABIC:
        return TextDirection::kRtl;
      default:
        break;
    }

    if (stop_at && stop_at(ch)) {
      break;
    }
  }
  return std::nullopt;
}

// static
std::optional<TextDirection> BidiParagraph::BaseDirectionForString(
    const StringView& text,
    bool (*stop_at)(UChar)) {
  return text.Is8Bit() ? BaseDirectionForString(text.Span8(), stop_at)
                       : BaseDirectionForString(text.Span16(), stop_at);
}

// static
String BidiParagraph::StringWithDirectionalOverride(const StringView& text,
                                                    TextDirection direction) {
  StringBuilder builder;
  builder.Reserve16BitCapacity(text.length() + 2);
  builder.Append(IsLtr(direction) ? kLeftToRightOverrideCharacter
                                  : kRightToLeftOverrideCharacter);
  builder.Append(text);
  builder.Append(kPopDirectionalFormattingCharacter);
  return builder.ToString();
}

unsigned BidiParagraph::GetLogicalRun(unsigned start, UBiDiLevel* level) const {
  int32_t end;
  ubidi_getLogicalRun(ubidi_.get(), start, &end, level);
  return end;
}

void BidiParagraph::GetLogicalRuns(const String& text, Runs* runs) const {
  DCHECK(runs->empty());
  for (unsigned start = 0; start < text.length();) {
    UBiDiLevel level;
    unsigned end = GetLogicalRun(start, &level);
    DCHECK_GT(end, start);
    runs->emplace_back(start, end, level);
    start = end;
  }
}

void BidiParagraph::GetVisualRuns(const String& text, Runs* runs) const {
  DCHECK(runs->empty());

  Runs logical_runs;
  GetLogicalRuns(text, &logical_runs);

  Vector<UBiDiLevel, 32> levels;
  levels.ReserveInitialCapacity(logical_runs.size());
  for (const Run& run : logical_runs) {
    levels.push_back(run.level);
  }
  Vector<int32_t, 32> indices_in_visual_order(logical_runs.size());
  IndicesInVisualOrder(levels, &indices_in_visual_order);

  for (int32_t index : indices_in_visual_order) {
    runs->push_back(logical_runs[index]);
  }
}

// static
void BidiParagraph::IndicesInVisualOrder(
    const Vector<UBiDiLevel, 32>& levels,
    Vector<int32_t, 32>* indices_in_visual_order_out) {
  // Check the size before passing the raw pointers to ICU.
  CHECK_EQ(levels.size(), indices_in_visual_order_out->size());
  ubidi_reorderVisual(levels.data(), levels.size(),
                      indices_in_visual_order_out->data());
}

}  // namespace blink
