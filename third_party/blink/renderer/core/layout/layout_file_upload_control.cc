/*
 * Copyright (C) 2006, 2007, 2012 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/layout_file_upload_control.h"

#include <math.h>
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/paint/file_upload_control_painter.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

namespace blink {

const int kDefaultWidthNumChars = 34;
const int kButtonShadowHeight = 2;

LayoutFileUploadControl::LayoutFileUploadControl(HTMLInputElement* input)
    : LayoutBlockFlow(input),
      can_receive_dropped_files_(input->CanReceiveDroppedFiles()) {}

LayoutFileUploadControl::~LayoutFileUploadControl() = default;

void LayoutFileUploadControl::UpdateFromElement() {
  HTMLInputElement* input = ToHTMLInputElement(GetNode());
  DCHECK_EQ(input->type(), input_type_names::kFile);

  if (HTMLInputElement* button = UploadButton()) {
    bool new_can_receive_dropped_files_state = input->CanReceiveDroppedFiles();
    if (can_receive_dropped_files_ != new_can_receive_dropped_files_state) {
      can_receive_dropped_files_ = new_can_receive_dropped_files_state;
      button->SetActive(new_can_receive_dropped_files_state);
    }
  }

  // This only supports clearing out the files, but that's OK because for
  // security reasons that's the only change the DOM is allowed to make.
  FileList* files = input->files();
  DCHECK(files);
  if (files && files->IsEmpty())
    SetShouldDoFullPaintInvalidation();
}

int LayoutFileUploadControl::MaxFilenameWidth() const {
  int upload_button_width =
      (UploadButton() && UploadButton()->GetLayoutBox())
          ? UploadButton()->GetLayoutBox()->PixelSnappedWidth()
          : 0;
  return std::max(0, PhysicalContentBoxRect().PixelSnappedWidth() -
                         upload_button_width - kAfterButtonSpacing);
}

void LayoutFileUploadControl::PaintObject(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) const {
  FileUploadControlPainter(*this).PaintObject(paint_info, paint_offset);
}

void LayoutFileUploadControl::ComputeIntrinsicLogicalWidths(
    LayoutUnit& min_logical_width,
    LayoutUnit& max_logical_width) const {
  // Figure out how big the filename space needs to be for a given number of
  // characters (using "0" as the nominal character).
  const UChar kCharacter = '0';
  const String character_as_string = String(&kCharacter, 1);
  const Font& font = StyleRef().GetFont();
  float min_default_label_width =
      kDefaultWidthNumChars *
      font.Width(ConstructTextRun(font, character_as_string, StyleRef(),
                                  TextRun::kAllowTrailingExpansion));

  const String label = ToHTMLInputElement(GetNode())->GetLocale().QueryString(
      IDS_FORM_FILE_NO_FILE_LABEL);
  float default_label_width = font.Width(ConstructTextRun(
      font, label, StyleRef(), TextRun::kAllowTrailingExpansion));
  if (HTMLInputElement* button = UploadButton()) {
    if (LayoutObject* button_layout_object = button->GetLayoutObject())
      default_label_width += button_layout_object->MaxPreferredLogicalWidth() +
                             kAfterButtonSpacing;
  }
  max_logical_width =
      LayoutUnit(ceilf(std::max(min_default_label_width, default_label_width)));

  if (!StyleRef().Width().IsPercentOrCalc())
    min_logical_width = max_logical_width;
}

void LayoutFileUploadControl::ComputePreferredLogicalWidths() {
  DCHECK(PreferredLogicalWidthsDirty());

  min_preferred_logical_width_ = LayoutUnit();
  max_preferred_logical_width_ = LayoutUnit();
  const ComputedStyle& style_to_use = StyleRef();

  if (style_to_use.Width().IsFixed() && style_to_use.Width().Value() > 0)
    min_preferred_logical_width_ = max_preferred_logical_width_ =
        AdjustContentBoxLogicalWidthForBoxSizing(
            LayoutUnit(style_to_use.Width().Value()));
  else
    ComputeIntrinsicLogicalWidths(min_preferred_logical_width_,
                                  max_preferred_logical_width_);

  if (style_to_use.MinWidth().IsFixed() &&
      style_to_use.MinWidth().Value() > 0) {
    max_preferred_logical_width_ =
        std::max(max_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     LayoutUnit(style_to_use.MinWidth().Value())));
    min_preferred_logical_width_ =
        std::max(min_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     LayoutUnit(style_to_use.MinWidth().Value())));
  }

  if (style_to_use.MaxWidth().IsFixed()) {
    max_preferred_logical_width_ =
        std::min(max_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     LayoutUnit(style_to_use.MaxWidth().Value())));
    min_preferred_logical_width_ =
        std::min(min_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     LayoutUnit(style_to_use.MaxWidth().Value())));
  }

  int to_add = BorderAndPaddingWidth().ToInt();
  min_preferred_logical_width_ += to_add;
  max_preferred_logical_width_ += to_add;

  ClearPreferredLogicalWidthsDirty();
}

PositionWithAffinity LayoutFileUploadControl::PositionForPoint(
    const PhysicalOffset&) const {
  return PositionWithAffinity();
}

HTMLInputElement* LayoutFileUploadControl::UploadButton() const {
  // FIXME: This should be on HTMLInputElement as an API like
  // innerButtonElement().
  HTMLInputElement* input = ToHTMLInputElement(GetNode());
  return ToHTMLInputElementOrNull(input->UserAgentShadowRoot()->firstChild());
}

String LayoutFileUploadControl::ButtonValue() {
  if (HTMLInputElement* button = UploadButton())
    return button->value();

  return String();
}

String LayoutFileUploadControl::FileTextValue() const {
  HTMLInputElement* input = ToHTMLInputElement(GetNode());
  DCHECK(input->files());
  return LayoutTheme::GetTheme().FileListNameForWidth(
      input->GetLocale(), input->files(), StyleRef().GetFont(),
      MaxFilenameWidth());
}

PhysicalRect LayoutFileUploadControl::ControlClipRect(
    const PhysicalOffset& additional_offset) const {
  PhysicalRect rect(additional_offset, Size());
  rect.Expand(BorderInsets());
  rect.offset.top -= LayoutUnit(kButtonShadowHeight);
  rect.size.height += LayoutUnit(kButtonShadowHeight) * 2;
  return rect;
}

// Override to allow effective ControlClipRect to be bigger than the padding
// box because of kButtonShadowHeight.
PhysicalRect LayoutFileUploadControl::OverflowClipRect(
    const PhysicalOffset& additional_offset,
    OverlayScrollbarClipBehavior) const {
  return ControlClipRect(additional_offset);
}

}  // namespace blink
