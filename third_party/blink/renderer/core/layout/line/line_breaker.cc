/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 *               All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/line/line_breaker.h"

#include "third_party/blink/renderer/core/layout/line/breaking_context_inline_headers.h"

namespace blink {

void LineBreaker::SkipLeadingWhitespace(InlineBidiResolver& resolver,
                                        LineInfo& line_info,
                                        LineWidth& width) {
  while (
      !resolver.GetPosition().AtEnd() &&
      !RequiresLineBox(resolver.GetPosition(), line_info, kLeadingWhitespace)) {
    LineLayoutItem line_layout_item =
        resolver.GetPosition().GetLineLayoutItem();
    if (line_layout_item.IsOutOfFlowPositioned()) {
      SetStaticPositions(block_, LineLayoutBox(line_layout_item),
                         width.IndentText());
      if (line_layout_item.StyleRef().IsOriginalDisplayInlineType()) {
        resolver.Runs().AddRun(
            CreateRun(0, 1, LineLayoutItem(line_layout_item), resolver));
        line_info.IncrementRunsFromLeadingWhitespace();
      }
    } else if (line_layout_item.IsFloating()) {
      block_.InsertFloatingObject(LineLayoutBox(line_layout_item));
      block_.PlaceNewFloats(block_.LogicalHeight(), &width);
    }
    resolver.GetPosition().Increment(&resolver);
  }
  resolver.CommitExplicitEmbedding(resolver.Runs());
}

void LineBreaker::Reset() {
  positioned_objects_.clear();
  hyphenated_ = false;
  clear_ = EClear::kNone;
}

InlineIterator LineBreaker::NextLineBreak(InlineBidiResolver& resolver,
                                          LineInfo& line_info,
                                          LayoutTextInfo& layout_text_info,
                                          WordMeasurements& word_measurements) {
  Reset();

  DCHECK(resolver.GetPosition().Root() == block_);

  bool applied_start_width = resolver.GetPosition().Offset() > 0;

  bool is_first_formatted_line =
      line_info.IsFirstLine() && block_.CanContainFirstFormattedLine();
  LineWidth width(block_, line_info.IsFirstLine(),
                  RequiresIndent(is_first_formatted_line));

  SkipLeadingWhitespace(resolver, line_info, width);

  if (resolver.GetPosition().AtEnd())
    return resolver.GetPosition();

  BreakingContext context(resolver, line_info, width, layout_text_info,
                          applied_start_width, block_);

  while (context.CurrentItem()) {
    context.InitializeForCurrentObject();
    if (context.CurrentItem().IsBR()) {
      context.HandleBR(clear_);
    } else if (context.CurrentItem().IsOutOfFlowPositioned()) {
      context.HandleOutOfFlowPositioned(positioned_objects_);
    } else if (context.CurrentItem().IsFloating()) {
      context.HandleFloat();
    } else if (context.CurrentItem().IsLayoutInline()) {
      context.HandleEmptyInline();
    } else if (context.CurrentItem().IsAtomicInlineLevel()) {
      context.HandleReplaced();
    } else if (context.CurrentItem().IsText()) {
      if (context.HandleText(word_measurements, hyphenated_)) {
        // We've hit a hard text line break. Our line break iterator is updated,
        // so go ahead and early return.
        return context.LineBreak();
      }
    } else {
      NOTREACHED();
    }

    if (context.AtEnd())
      return context.HandleEndOfLine();

    context.CommitAndUpdateLineBreakIfNeeded();

    if (context.AtEnd())
      return context.HandleEndOfLine();

    context.Increment();
  }

  context.ClearLineBreakIfFitsOnLine();

  return context.HandleEndOfLine();
}

}  // namespace blink
