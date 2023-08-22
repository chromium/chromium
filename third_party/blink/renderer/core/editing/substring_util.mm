/*
 * Copyright (C) 2005, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/substring_util.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/bridging.h"
#include "base/apple/scoped_cftyperef.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/mac/color_mac.h"

namespace blink {

namespace {

NSString* const kCrBaselineOffset = @"kCrBaselineOffset";

NSAttributedString* AttributedSubstringFromRange(LocalFrame* frame,
                                                 const EphemeralRange& range) {
  NSMutableAttributedString* string = [[NSMutableAttributedString alloc] init];
  NSMutableDictionary* attrs = [NSMutableDictionary dictionary];
  size_t length = range.EndPosition().ComputeOffsetInContainerNode() -
                  range.StartPosition().ComputeOffsetInContainerNode();

  unsigned position = 0;

  // TODO(editing-dev): The use of updateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  range.StartPosition().GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kEditing);

  for (TextIterator it(range.StartPosition(), range.EndPosition());
       !it.AtEnd() && [string length] < length; it.Advance()) {
    unsigned num_characters = it.length();
    if (!num_characters) {
      continue;
    }

    const Node& container = it.CurrentContainer();
    const LayoutObject* layout_object = container.GetLayoutObject();
    if (!layout_object) {
      continue;
    }

    // There are two ways that the size of text can be affected by the user. One
    // is the page scale factor, which is what the user changes by pinching on
    // the trackpad. The other is zooming, which combines 1. the effect of users
    // using ⌘+/⌘- and 2. implementing the device scale factor (this is an
    // implementation feature called "zoom-for-dsf"). The zoom value is baked
    // into the font size, so to calculate the font size needed for display, the
    // device scale factor must be divided out from the font size and the page
    // scale factor must be multiplied in.

    const ComputedStyle* style = layout_object->Style();
    const SimpleFontData* primaryFont = style->GetFont().PrimaryFont();
    const FontPlatformData& font_platform_data = primaryFont->PlatformData();

    const float page_scale_factor = frame->GetPage()->PageScaleFactor();
    const float device_scale_factor =
        frame->GetWidgetForLocalRoot()->DIPsToBlinkSpace(1.0f);

    attrs[kCrBaselineOffset] =
        @(primaryFont->GetFontMetrics().Descent() * page_scale_factor);

    NSFont* original_font =
        base::apple::CFToNSPtrCast(font_platform_data.CtFont());
    const CGFloat desired_size =
        font_platform_data.size() * page_scale_factor / device_scale_factor;

    NSFont* font = nil;
    if (original_font) {
      font = [original_font fontWithSize:desired_size];
    }

    // If the platform font can't be loaded, or the size is incorrect comparing
    // to the computed style, it's likely that the site is using a web font.
    // For now, just use the default font instead.
    // TODO(rsesek): Change the font activation flags to allow other processes
    // to use the font.
    // TODO(shuchen): Support scaling the font as necessary according to CSS
    // transforms, not just pinch-zoom.
    if (!font || floor(font_platform_data.size()) !=
                     floor(original_font.fontDescriptor.pointSize)) {
      font = [NSFont systemFontOfSize:style->GetFont()
                                          .GetFontDescription()
                                          .ComputedSize() *
                                      page_scale_factor / device_scale_factor];
    }
    attrs[NSFontAttributeName] = font;

    if (!style->VisitedDependentColor(GetCSSPropertyColor())
             .IsFullyTransparent()) {
      attrs[NSForegroundColorAttributeName] =
          NsColor(style->VisitedDependentColor(GetCSSPropertyColor()));
    } else {
      [attrs removeObjectForKey:NSForegroundColorAttributeName];
    }
    if (!style->VisitedDependentColor(GetCSSPropertyBackgroundColor())
             .IsFullyTransparent()) {
      attrs[NSBackgroundColorAttributeName] = NsColor(
          style->VisitedDependentColor(GetCSSPropertyBackgroundColor()));
    } else {
      [attrs removeObjectForKey:NSBackgroundColorAttributeName];
    }

    NSString* substring = it.GetTextState().GetTextForTesting();
    [string replaceCharactersInRange:NSMakeRange(position, 0)
                          withString:substring];
    [string setAttributes:attrs range:NSMakeRange(position, num_characters)];
    position += num_characters;
  }
  return string;
}

gfx::Point GetBaselinePoint(LocalFrameView* frame_view,
                            const EphemeralRange& range,
                            NSAttributedString* string) {
  gfx::Rect string_rect = frame_view->FrameToViewport(FirstRectForRange(range));
  gfx::Point string_point = string_rect.bottom_left();

  // Adjust for the font's descender. AppKit wants the baseline point.
  if (string.length) {
    NSDictionary* attributes = [string attributesAtIndex:0
                                          effectiveRange:nullptr];
    if (NSNumber* descender = attributes[kCrBaselineOffset]) {
      string_point.Offset(0, -ceil(descender.doubleValue));
    }
  }
  return string_point;
}

}  // namespace

base::apple::ScopedCFTypeRef<CFAttributedStringRef>
SubstringUtil::AttributedWordAtPoint(WebFrameWidgetImpl* frame_widget,
                                     gfx::Point point,
                                     gfx::Point& baseline_point) {
  HitTestResult result = frame_widget->CoreHitTestResultAt(gfx::PointF(point));

  if (!result.InnerNode()) {
    return base::apple::ScopedCFTypeRef<CFAttributedStringRef>();
  }
  LocalFrame* frame = result.InnerNode()->GetDocument().GetFrame();
  EphemeralRange range =
      frame->GetEditor().RangeForPoint(result.RoundedPointInInnerNodeFrame());
  if (range.IsNull()) {
    return base::apple::ScopedCFTypeRef<CFAttributedStringRef>();
  }

  // Expand to word under point.
  const SelectionInDOMTree selection = ExpandWithGranularity(
      SelectionInDOMTree::Builder().SetBaseAndExtent(range).Build(),
      TextGranularity::kWord);
  const EphemeralRange word_range = NormalizeRange(selection);

  // Convert to CFAttributedStringRef.
  NSAttributedString* string = AttributedSubstringFromRange(frame, word_range);
  baseline_point = GetBaselinePoint(frame->View(), word_range, string);
  return base::apple::ScopedCFTypeRef<CFAttributedStringRef>(
      base::apple::NSToCFOwnershipCast(string));
}

base::apple::ScopedCFTypeRef<CFAttributedStringRef>
SubstringUtil::AttributedSubstringInRange(LocalFrame* frame,
                                          wtf_size_t location,
                                          wtf_size_t length,
                                          gfx::Point& baseline_point) {
  frame->View()->UpdateStyleAndLayout();

  Element* editable = frame->Selection().RootEditableElementOrDocumentElement();
  if (!editable) {
    return base::apple::ScopedCFTypeRef<CFAttributedStringRef>();
  }
  const EphemeralRange ephemeral_range(
      PlainTextRange(location, location + length).CreateRange(*editable));
  if (ephemeral_range.IsNull()) {
    return base::apple::ScopedCFTypeRef<CFAttributedStringRef>();
  }

  NSAttributedString* string =
      AttributedSubstringFromRange(frame, ephemeral_range);
  baseline_point = GetBaselinePoint(frame->View(), ephemeral_range, string);
  return base::apple::ScopedCFTypeRef<CFAttributedStringRef>(
      base::apple::NSToCFOwnershipCast(string));
}

}  // namespace blink
