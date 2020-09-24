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
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/paint/file_upload_control_painter.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/string_truncator.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

namespace blink {

const int kButtonShadowHeight = 2;

LayoutFileUploadControl::LayoutFileUploadControl(Element* input)
    : LayoutBlockFlow(input) {
  DCHECK_EQ(To<HTMLInputElement>(input)->type(), input_type_names::kFile);
}

LayoutFileUploadControl::~LayoutFileUploadControl() = default;

bool LayoutFileUploadControl::IsChildAllowed(LayoutObject* child,
                                             const ComputedStyle& style) const {
  const Node* child_node = child->GetNode();
  // Reject shadow nodes other than UploadButton.
  if (child_node && child_node->OwnerShadowHost() == GetNode() &&
      child_node != UploadButton())
    return false;
  return LayoutBlockFlow::IsChildAllowed(child, style);
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

HTMLInputElement* LayoutFileUploadControl::UploadButton() const {
  return To<HTMLInputElement>(GetNode())->UploadButton();
}

String LayoutFileUploadControl::FileTextValue() const {
  int width = MaxFilenameWidth();
  if (width <= 0)
    return String();
  auto* input = To<HTMLInputElement>(GetNode());
  DCHECK(input->files());
  String text = input->FileStatusText();
  if (input->files()->length() >= 2)
    return StringTruncator::RightTruncate(text, width, StyleRef().GetFont());
  return StringTruncator::CenterTruncate(text, width, StyleRef().GetFont());
}

// Override to allow effective clip rect to be bigger than the padding box
// because of kButtonShadowHeight.
PhysicalRect LayoutFileUploadControl::OverflowClipRect(
    const PhysicalOffset& additional_offset,
    OverlayScrollbarClipBehavior) const {
  PhysicalRect rect(additional_offset, Size());
  rect.Expand(BorderInsets());
  rect.offset.top -= LayoutUnit(kButtonShadowHeight);
  rect.size.height += LayoutUnit(kButtonShadowHeight) * 2;
  return rect;
}

}  // namespace blink
