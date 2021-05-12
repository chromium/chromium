/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All right reserved.
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

#include "third_party/blink/renderer/core/layout/bidi_run_for_line.h"

#include "third_party/blink/renderer/core/layout/line/inline_iterator.h"

namespace blink {

static LineLayoutItem FirstLayoutObjectForDirectionalityDetermination(
    LineLayoutItem root,
    LineLayoutItem current = nullptr) {
  LineLayoutItem next = current;
  while (current) {
    if (TreatAsIsolated(current.StyleRef()) &&
        (current.IsLayoutInline() || current.IsLayoutBlock())) {
      if (current != root)
        current = nullptr;
      else
        current = next;
      break;
    }
    current = current.Parent();
  }

  if (!current)
    current = root.SlowFirstChild();

  while (current) {
    next = nullptr;
    if (IsIteratorTarget(current) &&
        !(current.IsText() &&
          LineLayoutText(current).IsAllCollapsibleWhitespace()))
      break;

    if (!IsIteratorTarget(LineLayoutItem(current)) &&
        !TreatAsIsolated(current.StyleRef()))
      next = current.SlowFirstChild();

    if (!next) {
      while (current && current != root) {
        next = current.NextSibling();
        if (next)
          break;
        current = current.Parent();
      }
    }

    if (!next)
      break;

    current = next;
  }

  return current;
}

TextDirection DeterminePlaintextDirectionality(LineLayoutItem root,
                                               LineLayoutItem current,
                                               unsigned pos) {
  LineLayoutItem first_layout_object =
      FirstLayoutObjectForDirectionalityDetermination(root, current);
  InlineIterator iter(LineLayoutItem(root), first_layout_object,
                      first_layout_object == current ? pos : 0);
  InlineBidiResolver observer;
  observer.SetStatus(BidiStatus(root.StyleRef().Direction(),
                                IsOverride(root.StyleRef().GetUnicodeBidi())));
  observer.SetPositionIgnoringNestedIsolates(iter);
  return observer.DetermineParagraphDirectionality();
}

static inline void SetupResolverToResumeInIsolate(InlineBidiResolver& resolver,
                                                  LineLayoutItem root,
                                                  LineLayoutItem start_object) {
  if (root != start_object) {
    LineLayoutItem parent = start_object.Parent();
    SetupResolverToResumeInIsolate(resolver, root, parent);
    NotifyObserverEnteredObject(&resolver, LineLayoutItem(start_object));
  }
}

void ConstructBidiRunsForLine(InlineBidiResolver& top_resolver,
                              BidiRunList<BidiRun>& bidi_runs,
                              const InlineIterator& end_of_line,
                              VisualDirectionOverride override,
                              bool previous_line_broke_cleanly,
                              bool is_new_uba_paragraph) {
  // FIXME: We should pass a BidiRunList into createBidiRunsForLine instead
  // of the resolver owning the runs.
  DCHECK_EQ(&top_resolver.Runs(), &bidi_runs);
  DCHECK(top_resolver.GetPosition() != end_of_line);
  LineLayoutItem current_root = top_resolver.GetPosition().Root();
  top_resolver.CreateBidiRunsForLine(end_of_line, override,
                                     previous_line_broke_cleanly);

  while (!top_resolver.IsolatedRuns().IsEmpty()) {
    // It does not matter which order we resolve the runs as long as we
    // resolve them all.
    BidiIsolatedRun isolated_run = top_resolver.IsolatedRuns().back();
    top_resolver.IsolatedRuns().pop_back();
    current_root = isolated_run.root;

    LineLayoutItem start_obj = isolated_run.object;

    // Only inlines make sense with unicode-bidi: isolate (blocks are
    // already isolated).
    // FIXME: Because enterIsolate is not passed a LayoutObject, we have to
    // crawl up the tree to see which parent inline is the isolate. We could
    // change enterIsolate to take a LayoutObject and do this logic there,
    // but that would be a layering violation for BidiResolver (which knows
    // nothing about LayoutObject).
    LineLayoutItem isolated_inline =
        HighestContainingIsolateWithinRoot(start_obj, current_root);
    DCHECK(isolated_inline);

    InlineBidiResolver isolated_resolver;
    LineMidpointState& isolated_line_midpoint_state =
        isolated_resolver.GetMidpointState();
    isolated_line_midpoint_state =
        top_resolver.MidpointStateForIsolatedRun(isolated_run.run_to_replace);
    UnicodeBidi unicode_bidi = isolated_inline.StyleRef().GetUnicodeBidi();
    TextDirection direction;
    if (unicode_bidi == UnicodeBidi::kPlaintext) {
      direction = DeterminePlaintextDirectionality(
          isolated_inline, is_new_uba_paragraph ? start_obj : nullptr);
    } else {
      DCHECK(unicode_bidi == UnicodeBidi::kIsolate ||
             unicode_bidi == UnicodeBidi::kIsolateOverride);
      direction = isolated_inline.StyleRef().Direction();
    }
    isolated_resolver.SetStatus(BidiStatus::CreateForIsolate(
        direction, IsOverride(unicode_bidi), isolated_run.level));

    SetupResolverToResumeInIsolate(isolated_resolver, isolated_inline,
                                   start_obj);

    // The starting position is the beginning of the first run within the
    // isolate that was identified during the earlier call to
    // createBidiRunsForLine. This can be but is not necessarily the first
    // run within the isolate.
    InlineIterator iter =
        InlineIterator(LineLayoutItem(isolated_inline),
                       LineLayoutItem(start_obj), isolated_run.position);
    isolated_resolver.SetPositionIgnoringNestedIsolates(iter);
    // We stop at the next end of line; we may re-enter this isolate in the
    // next call to constructBidiRuns().
    // FIXME: What should end and previousLineBrokeCleanly be?
    // rniwa says previousLineBrokeCleanly is just a WinIE hack and could
    // always be false here?
    isolated_resolver.CreateBidiRunsForLine(end_of_line, kNoVisualOverride,
                                            previous_line_broke_cleanly);

    DCHECK(isolated_resolver.Runs().RunCount());
    if (isolated_resolver.Runs().RunCount())
      bidi_runs.ReplaceRunWithRuns(&isolated_run.run_to_replace,
                                   isolated_resolver.Runs());

    // If we encountered any nested isolate runs, save them for later
    // processing.
    while (!isolated_resolver.IsolatedRuns().IsEmpty()) {
      BidiIsolatedRun run_with_context =
          isolated_resolver.IsolatedRuns().back();
      isolated_resolver.IsolatedRuns().pop_back();
      top_resolver.SetMidpointStateForIsolatedRun(
          run_with_context.run_to_replace,
          isolated_resolver.MidpointStateForIsolatedRun(
              run_with_context.run_to_replace));
      top_resolver.IsolatedRuns().push_back(run_with_context);
    }
  }
}

}  // namespace blink
