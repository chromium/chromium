/*
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TEXT_CONTROL_MULTI_LINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TEXT_CONTROL_MULTI_LINE_H_

#include "third_party/blink/renderer/core/layout/layout_text_control.h"

namespace blink {

class HTMLTextAreaElement;

class LayoutTextControlMultiLine final : public LayoutTextControl {
 public:
  LayoutTextControlMultiLine(HTMLTextAreaElement*);
  ~LayoutTextControlMultiLine() override;

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectTextArea || LayoutTextControl::IsOfType(type);
  }

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) override;

  LayoutUnit PreferredContentLogicalWidth(float char_width) const override;
  LayoutUnit ComputeControlLogicalHeight(
      LayoutUnit line_height,
      LayoutUnit non_content_height) const override;
  // We override the two baseline functions because we want our baseline to be
  // the bottom of our margin box.
  LayoutUnit BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const override {
    return LayoutUnit(-1);
  }

  LayoutObject* LayoutSpecialExcludedChild(bool relayout_children,
                                           SubtreeLayoutScope&) override;

  LayoutUnit ScrollWidth() const override;
  LayoutUnit ScrollHeight() const override;
};

}  // namespace blink

#endif
