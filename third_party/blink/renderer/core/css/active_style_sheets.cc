// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/active_style_sheets.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Returns true if the entries from `candidate` which also occur in `sequence`
// can all be found in the same order in `sequence`. Entries which do not occur
// in `sequence` at all are skipped, since only entries present on both sides
// can say anything about their relative order.
bool CommonEntriesAreSubsequence(
    base::span<const ActiveStyleSheet> candidate,
    base::span<const ActiveStyleSheet> sequence,
    base::span<const ActiveStyleSheet> sequence_sorted) {
  for (const ActiveStyleSheet& entry : candidate) {
    if (!std::ranges::binary_search(sequence_sorted, entry)) {
      continue;
    }
    while (!sequence.empty() && sequence.front() != entry) {
      sequence = sequence.subspan(1u);
    }
    if (sequence.empty()) {
      return false;
    }
    sequence = sequence.subspan(1u);
  }
  return true;
}

// Returns true if entries which occur on both sides may have changed their
// relative order. Both spans are full middles; only the entries which occur on
// both are compared, as the rest say nothing about order.
//
// If no duplicate was removed, every entry the old side shares with the new one
// still has a match, so those entries form a subsequence of the new side
// exactly when nothing moved, and likewise the other way around when no
// duplicate was added.
bool CommonEntriesMayHaveBeenReordered(
    base::span<const ActiveStyleSheet> old_middle,
    base::span<const ActiveStyleSheet> new_middle,
    base::span<const ActiveStyleSheet> old_sorted,
    base::span<const ActiveStyleSheet> new_sorted,
    bool removed_duplicate_entry,
    bool added_duplicate_entry) {
  if (!removed_duplicate_entry) {
    return !CommonEntriesAreSubsequence(old_middle, new_middle, new_sorted);
  }
  if (!added_duplicate_entry) {
    return !CommonEntriesAreSubsequence(new_middle, old_middle, old_sorted);
  }
  // Duplicates were both added and removed, so which occurrences survived is
  // ambiguous. Conservatively report a reorder.
  return true;
}

}  // namespace

ActiveSheetsChange CompareActiveStyleSheets(
    const ActiveStyleSheetVector& old_style_sheets,
    const ActiveStyleSheetVector& new_style_sheets,
    const HeapVector<Member<RuleSetDiff>>& diffs,
    HeapHashSet<Member<RuleSet>>& changed_rule_sets) {
  unsigned new_style_sheet_count = new_style_sheets.size();
  unsigned old_style_sheet_count = old_style_sheets.size();

  unsigned min_count = std::min(new_style_sheet_count, old_style_sheet_count);
  unsigned index = 0;

  // Walk the common prefix of stylesheets. If the stylesheet rules were
  // modified since last time, add them to the list of changed rulesets.
  for (; index < min_count &&
         new_style_sheets[index].first == old_style_sheets[index].first;
       ++index) {
    if (new_style_sheets[index].second == old_style_sheets[index].second) {
      continue;
    }

    // See if we can do better than inserting the entire old and the entire
    // new ruleset; if we have a RuleSetDiff describing their diff better,
    // we can use that instead, presumably with fewer rules (there will never
    // be more, but there are also cases where there could be the same number).
    // Note that CreateDiffRuleset() can fail, i.e., return nullptr, in which
    // case we fall back to the non-diff path.)
    RuleSet* diff_ruleset = nullptr;
    if (new_style_sheets[index].second && old_style_sheets[index].second) {
      for (const RuleSetDiff* diff : diffs) {
        if (diff->Matches(old_style_sheets[index].second,
                          new_style_sheets[index].second)) {
          diff_ruleset = diff->CreateDiffRuleset();
          break;
        }
      }
    }

    if (diff_ruleset) {
      changed_rule_sets.insert(diff_ruleset);
    } else {
      if (new_style_sheets[index].second) {
        changed_rule_sets.insert(new_style_sheets[index].second);
      }
      if (old_style_sheets[index].second) {
        changed_rule_sets.insert(old_style_sheets[index].second);
      }
    }
  }

  // If we add a sheet for which the media attribute currently doesn't match, we
  // have a null RuleSet and there's no need to do any style invalidation.
  // However, we need to tell the StyleEngine to re-collect viewport and device
  // dependent media query results so that we can correctly update active style
  // sheets when such media query evaluations change.
  bool adds_non_matching_mq = false;

  if (index == old_style_sheet_count) {
    // The old stylesheet vector is a prefix of the new vector in terms of
    // StyleSheets. If none of the RuleSets changed, we only need to add the new
    // sheets to the ScopedStyleResolver (ActiveSheetsAppended).
    bool rule_sets_changed_in_common_prefix = !changed_rule_sets.empty();
    for (; index < new_style_sheet_count; ++index) {
      if (new_style_sheets[index].second) {
        changed_rule_sets.insert(new_style_sheets[index].second);
      } else if (new_style_sheets[index].first->HasMediaQueryResults()) {
        adds_non_matching_mq = true;
      }
    }
    if (rule_sets_changed_in_common_prefix) {
      return kActiveSheetsChanged;
    }
    if (changed_rule_sets.empty() && !adds_non_matching_mq) {
      return kNoActiveSheetsChanged;
    }
    return kActiveSheetsAppended;
  }

  if (index == new_style_sheet_count) {
    // Sheets removed from the end.
    for (; index < old_style_sheet_count; ++index) {
      if (old_style_sheets[index].second) {
        changed_rule_sets.insert(old_style_sheets[index].second);
      } else if (old_style_sheets[index].first->HasMediaQueryResults()) {
        adds_non_matching_mq = true;
      }
    }
    return changed_rule_sets.empty() && !adds_non_matching_mq
               ? kNoActiveSheetsChanged
               : kActiveSheetsChanged;
  }

  DCHECK_LT(index, old_style_sheet_count);
  DCHECK_LT(index, new_style_sheet_count);

  // Both the new and old active stylesheet vectors have stylesheets following
  // the common prefix. Trailing entries which are the same on both sides keep
  // both their rules and their relative order, so leave them out the same way
  // the common prefix is left out. This keeps the work below proportional to
  // the entries which actually moved rather than to the length of the vectors.
  CHECK_LT(index, min_count);
  unsigned common_suffix_length = 0;
  while (common_suffix_length < min_count - index) {
    CHECK_LT(common_suffix_length, old_style_sheet_count);
    CHECK_LT(common_suffix_length, new_style_sheet_count);
    if (old_style_sheets[old_style_sheet_count - 1 - common_suffix_length] !=
        new_style_sheets[new_style_sheet_count - 1 - common_suffix_length]) {
      break;
    }
    ++common_suffix_length;
  }

  // An entry which appears the same number of times on both sides contributes
  // the same rules in both, so find the entries which do not by sorting each
  // side and walking the two in lockstep. Sorting the sides separately keeps
  // track of which vector an entry came from, which matters when the same
  // stylesheet appears more than once.
  CHECK_LE(common_suffix_length, min_count - index);
  const wtf_size_t old_middle_size =
      old_style_sheet_count - index - common_suffix_length;
  const wtf_size_t new_middle_size =
      new_style_sheet_count - index - common_suffix_length;
  const base::span<const ActiveStyleSheet> old_middle =
      base::span(old_style_sheets).subspan(index, old_middle_size);
  const base::span<const ActiveStyleSheet> new_middle =
      base::span(new_style_sheets).subspan(index, new_middle_size);

  HeapVector<ActiveStyleSheet> old_sorted;
  old_sorted.reserve(old_middle_size);
  old_sorted.append_range(old_middle);
  std::sort(old_sorted.begin(), old_sorted.end());

  HeapVector<ActiveStyleSheet> new_sorted;
  new_sorted.reserve(new_middle_size);
  new_sorted.append_range(new_middle);
  std::sort(new_sorted.begin(), new_sorted.end());
  bool removed_duplicate_entry = false;
  bool added_duplicate_entry = false;

  const ActiveStyleSheet* last_matched = nullptr;

  // Records an entry which the merge below could not match on the other side,
  // meaning it was removed, inserted, or its active rules changed due to DOM,
  // CSSOM, or media query changes.
  auto add_changed = [&](const ActiveStyleSheet& active_sheet,
                         bool& changed_duplicate_entry) {
    if (last_matched && *last_matched == active_sheet) {
      changed_duplicate_entry = true;
    }
    if (active_sheet.second) {
      changed_rule_sets.insert(active_sheet.second);
    } else if (active_sheet.first->HasMediaQueryResults()) {
      adds_non_matching_mq = true;
    }
  };

  base::span<const ActiveStyleSheet> old_rest(old_sorted);
  base::span<const ActiveStyleSheet> new_rest(new_sorted);
  while (!old_rest.empty() && !new_rest.empty()) {
    if (old_rest.front() == new_rest.front()) {
      // Same sheet with the same rule set on both sides.
      last_matched = &old_rest.front();
      old_rest = old_rest.subspan(1u);
      new_rest = new_rest.subspan(1u);
    } else if (old_rest.front() < new_rest.front()) {
      add_changed(old_rest.front(), removed_duplicate_entry);
      old_rest = old_rest.subspan(1u);
    } else {
      add_changed(new_rest.front(), added_duplicate_entry);
      new_rest = new_rest.subspan(1u);
    }
  }
  for (const ActiveStyleSheet& active_sheet : old_rest) {
    add_changed(active_sheet, removed_duplicate_entry);
  }
  for (const ActiveStyleSheet& active_sheet : new_rest) {
    add_changed(active_sheet, added_duplicate_entry);
  }

  // The merge above does not determine whether the common entries kept their
  // relative order, so compare them directly.
  const bool common_entries_may_have_been_reordered =
      CommonEntriesMayHaveBeenReordered(old_middle, new_middle, old_sorted,
                                        new_sorted, removed_duplicate_entry,
                                        added_duplicate_entry);

  // Conservatively invalidate every old rule set between the common prefix and
  // the common suffix, which covers all of the sheets whose cascade position
  // may have changed. Sheets which are only in the new vector were already
  // invalidated above, and the surviving ones share their rule set with the old
  // vector.
  if (common_entries_may_have_been_reordered) {
    for (const ActiveStyleSheet& active_sheet : old_middle) {
      if (active_sheet.second) {
        changed_rule_sets.insert(active_sheet.second);
      }
    }
  }

  return changed_rule_sets.empty() && !adds_non_matching_mq
             ? kNoActiveSheetsChanged
             : kActiveSheetsChanged;
}

namespace {

bool HasMediaQueries(const ActiveStyleSheetVector& active_style_sheets) {
  for (const auto& active_sheet : active_style_sheets) {
    if (const MediaQuerySet* media_queries =
            active_sheet.first->MediaQueries()) {
      if (!media_queries->QueryVector().empty()) {
        return true;
      }
    }
    StyleSheetContents* contents = active_sheet.first->Contents();
    if (contents->HasMediaQueries()) {
      return true;
    }
  }
  return false;
}

bool HasSizeDependentMediaQueries(
    const ActiveStyleSheetVector& active_style_sheets) {
  for (const auto& active_sheet : active_style_sheets) {
    if (active_sheet.first->HasMediaQueryResults()) {
      return true;
    }
    StyleSheetContents* contents = active_sheet.first->Contents();
    if (!contents->HasRuleSet()) {
      continue;
    }
    if (contents->GetRuleSet().Features().HasMediaQueryResults()) {
      return true;
    }
  }
  return false;
}

bool HasDynamicViewportDependentMediaQueries(
    const ActiveStyleSheetVector& active_style_sheets) {
  for (const auto& active_sheet : active_style_sheets) {
    if (active_sheet.first->HasDynamicViewportDependentMediaQueries()) {
      return true;
    }
    StyleSheetContents* contents = active_sheet.first->Contents();
    if (!contents->HasRuleSet()) {
      continue;
    }
    if (contents->GetRuleSet()
            .Features()
            .HasDynamicViewportDependentMediaQueries()) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool AffectedByMediaValueChange(const ActiveStyleSheetVector& active_sheets,
                                MediaValueChange change) {
  if (change == MediaValueChange::kSize) {
    return HasSizeDependentMediaQueries(active_sheets);
  }
  if (change == MediaValueChange::kDynamicViewport) {
    return HasDynamicViewportDependentMediaQueries(active_sheets);
  }

  DCHECK(change == MediaValueChange::kOther);
  return HasMediaQueries(active_sheets);
}

}  // namespace blink
