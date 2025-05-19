// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/flex/flex_line_breaker.h"

#include "base/numerics/clamped_math.h"

namespace blink {

namespace {

// LayoutUnit is int32_t internally - scoring is done based on int32_t * int32_t
// which effectively can't saturate uint64_t given how many items we expect.
// (We'd OOM well before reaching this limit).
// Use base::ClampedNumeric to prevent overflow/etc.
using ScoreUnit = base::ClampedNumeric<uint64_t>;
static const ScoreUnit kInfinity = std::numeric_limits<uint64_t>::max();
static const ScoreUnit kZero = static_cast<uint64_t>(0);

// This problem isn't a new problem in computer science, however when working
// through the various different solutions to this they are either unintuitive,
// or complex.
//
// This is effectively the Knuth-Plass line-breaking[1] for flex-items.
// The penalty for each line is the square of the free-space.
//
// Naively this is O(N*N) complexity, however other approaches exist namely
// Hirschberg-Larmore "The least weight subsequence problem"[2].
// Go has an implementation[3] of this for wrapping comments.
//
// TL;DR I don't like writing algorithms which I can't easily understand myself
// as someday there will be a bug-report which we'll have to fix for them.
//
// So this is hopefully a (relatively) simple performant solution to this
// problem.
//
// Score(end, limit, ctx) returns the best score with a line-break after `end`.
//
// There are a few tricks to get this performant.
//  1. We'll lazily compute the score for a particular `end` (other algorithms
//     compute everything). Often there many values for `end` which the
//     solution is terrible, and not needed, or not worth calculating.
//     After the algorithm has run you can inspect the result, and see which
//     entries have `best_break == kNotFound`.
//  2. For a given end, we calculate (ahead of time) the earliest start index
//     (e.g. the largest number of items which will fit on this line).
//     This is important as we start our search here, which will have the
//     lowest score for the items on the line (the `line_score`).
//  3. We provide a `limit` value. This allows the algorithm to abort out of
//     the inner loop early if we know the solution for a given limit.
//
// [1] https://en.wikipedia.org/wiki/Knuth%E2%80%93Plass_line-breaking_algorithm
// [2] https://doi.org/10.1109/SFCS.1985.60
// [3] https://github.com/golang/go/blob/master/src/go/doc/comment/text.go
struct ScoreData {
  ScoreUnit limit = kInfinity;
  ScoreUnit best_score = kInfinity;
  wtf_size_t best_break = kNotFound;
  wtf_size_t start = kNotFound;
  wtf_size_t initial_start = kNotFound;
};

struct ScoreContext {
  const ScoreUnit line_break_size;
  const ScoreUnit gap_between_items;
  Vector<ScoreUnit> sums;
  Vector<ScoreData> scores;
};

ScoreUnit Score(wtf_size_t end, ScoreUnit limit, ScoreContext& ctx) {
  ScoreData& data = ctx.scores[end];

  ScoreUnit best_score = kInfinity;
  wtf_size_t best_break = kNotFound;
  wtf_size_t start = data.initial_start;

  // Check if we've already calculated the score for this `end`.
  if (data.limit == kInfinity) {
    if (data.best_score != kInfinity) {
      return data.best_score;
    }
  } else {
    if (limit <= data.limit) {
      return data.best_score;
    } else {
      best_score = data.best_score;
      best_break = data.best_break;
      start = data.start;
    }
  }

  for (; start <= end; ++start) {
    // Score this break - ensure that a single item that goes over the length
    // gets a score of zero.
    const ScoreUnit length = ctx.sums[end] -
                             (start == 0 ? kZero : ctx.sums[start - 1]) -
                             ctx.gap_between_items;
    const ScoreUnit line_score =
        length > ctx.line_break_size
            ? kZero
            : (ctx.line_break_size - length) * (ctx.line_break_size - length);

    // The score for this line is worse than the previous *total* best score,
    // as the line-score values monotonically increasing we can break from
    // this loop, as nothing else can beat the best total score.
    if (line_score > best_score) {
      break;
    }

    // The score for this line is worse than our current calculation limit.
    // Nothing else in the loop can be better as the line-score values are
    // monotonically increasing. Save the current loop state (so that if we
    // need to revisit this potential break with a higher limit).
    if (line_score > limit) {
      data.best_score = best_score;
      data.best_break = best_break;
      data.limit = limit;
      data.start = start;
      return best_score;
    }

    // Score this breakpoint, and update if better.
    const ScoreUnit new_limit =
        best_score == kInfinity ? kInfinity : best_score - line_score;
    const ScoreUnit score =
        line_score + (start == 0 ? kZero : Score(start - 1, new_limit, ctx));
    if (score <= best_score) {
      best_score = score;
      best_break = (start == 0) ? kNotFound : start - 1;
    }
  }

  data.best_score = best_score;
  data.best_break = best_break;
  data.limit = kInfinity;
  data.start = kNotFound;
  return data.best_score;
}

template <typename ShouldBreakFunc>
FlexLineBreakerResult BreakIntoLines(base::span<FlexItem> all_items,
                                     const LayoutUnit gap_between_items,
                                     const ShouldBreakFunc should_break) {
  HeapVector<InitialFlexLine, 1> flex_lines;
  LayoutUnit max_sum_hypothetical_main_size;

  base::span<FlexItem> items = all_items;

  while (!items.empty()) {
    LayoutUnit sum_flex_base_size;
    LayoutUnit sum_hypothetical_main_size;
    wtf_size_t count = 0u;

    for (auto& item : items) {
      if (should_break(count, sum_hypothetical_main_size +
                                  item.HypotheticalMainAxisMarginBoxSize())) {
        break;
      }

      sum_flex_base_size += item.FlexBaseMarginBoxSize() + gap_between_items;
      sum_hypothetical_main_size +=
          item.HypotheticalMainAxisMarginBoxSize() + gap_between_items;
      ++count;
    }
    // Take off the last gap (note we *always* have an item in the line).
    sum_hypothetical_main_size -= gap_between_items;
    sum_flex_base_size -= gap_between_items;

    auto [line_items, remaining_items] = items.split_at(count);
    flex_lines.emplace_back(line_items, sum_flex_base_size,
                            sum_hypothetical_main_size);
    max_sum_hypothetical_main_size =
        std::max(max_sum_hypothetical_main_size, sum_hypothetical_main_size);
    items = remaining_items;
  }

  return {flex_lines, max_sum_hypothetical_main_size};
}

}  // namespace

FlexLineBreakerResult BalanceBreakFlexItemsIntoLines(
    base::span<FlexItem> all_items,
    const LayoutUnit line_break_size,
    const LayoutUnit gap_between_items,
    wtf_size_t min_line_count) {
  ScoreContext ctx = {
      .line_break_size = static_cast<uint64_t>(line_break_size.RawValue()),
      .gap_between_items = static_cast<uint64_t>(gap_between_items.RawValue()),
      .sums = Vector<ScoreUnit>(all_items.size(), 0u),
      .scores = Vector(all_items.size(), ScoreData())};

  // Build up the prefix-sums[1], and work out how many lines we have.
  //
  // For example if we have the item sizes:
  //   [10, 20, 10]
  // The prefix sums array would be:
  //   [10, 30, 40]
  // This allows us to quickly determine the sum of items [i, j] via:
  //   sums[j] - sums[i-1]
  //
  // [1] https://en.wikipedia.org/wiki/Prefix_sum
  {
    for (wtf_size_t i = 0u; i < all_items.size(); ++i) {
      // NOTE: The prefix-sums array needs to be strictly monotonically
      // increasing for any of the "fast" balancing algorithms to work
      // correctly. Negative margins can potentially break this assumption.
      //
      // To guard against this (at the moment) we just clamp the margin-box
      // size to zero when this situation occurs. There are potentially more
      // advanced schemes (e.g. a forward scan through any negative items,
      // which reduce the size of the current item); but we'll start with this
      // simple approximation and see what feedback we get.
      const LayoutUnit item_size = all_items[i]
                                       .HypotheticalMainAxisMarginBoxSize()
                                       .ClampNegativeToZero();
      ctx.sums[i] = (i == 0 ? kZero : ctx.sums[i - 1]) +
                    (item_size + gap_between_items).RawValue();
    }
  }

  // Using the prefix-sums array returns how many flex-lines would result with
  // greedy packing given `line_break_size`.
  auto greedy_line_count = [&ctx](ScoreUnit line_break_size) -> wtf_size_t {
    const auto begin = ctx.sums.begin();
    const auto end = ctx.sums.end();
    auto it = begin;
    ScoreUnit previous_sum = kZero;
    wtf_size_t line_count = 1u;
    for (;;) {
      // Get the first value that has a size *greater* than the breakpoint.
      auto next = std::upper_bound(
          it, end, previous_sum + line_break_size + ctx.gap_between_items);

      // Check if we are at the end, there is no additional line.
      if (next == end) {
        break;
      }

      // `next` is past our breakpoint. There are two scenarios:
      //  - We are a single item which exceeds the line-break size, don't
      //    backtrack for this case.
      //  - There is more than one item, backtrack one item so the content fits
      //    on the line.
      const bool is_single_item =
          ((line_count == 1 ? 1 : 0) + std::distance(it, next)) <= 1;
      it = is_single_item ? next : std::prev(next);
      previous_sum = *it;

      // Only increment our line-count if we have content following `it`.
      if (std::next(it) != end) {
        ++line_count;
      }
    }
    return line_count;
  };

  const wtf_size_t line_count = greedy_line_count(ctx.line_break_size);

  // TODO(ikilpatrick): Bisect `line_break_size` to a smaller value with the
  // same number of lines. We also need this for the "min-lines" feature.
  // NOTE: This is close to being balanced, but not quite.

  // Next we can calculate for a given end index, what is the earliest start
  // index which will fit on the line.
  {
    wtf_size_t i = 0;
    for (wtf_size_t j = 0; j < all_items.size(); ++j) {
      DCHECK_LE(i, j);

      auto length = [&ctx](wtf_size_t i, wtf_size_t j) -> ScoreUnit {
        return ctx.sums[j] - (i == 0 ? kZero : ctx.sums[i - 1]) -
               ctx.gap_between_items;
      };

      // For this end, find the earliest start which fits on the line.
      while (i < j && length(i, j) > ctx.line_break_size) {
        ++i;
      }
      ctx.scores[j].initial_start = i;

      // There is a single item which is larger than the line-break size. Skip
      // past it for the next line.
      if (i == j && length(i, j) > ctx.line_break_size) {
        ++i;
      }
    }
  }

  Score(all_items.size() - 1, kInfinity, ctx);

  // Next retrieve the number of items on each line (in reverse).
  Vector<wtf_size_t> item_counts;
  {
    item_counts.ReserveInitialCapacity(line_count);
    wtf_size_t previous_index = all_items.size() - 1;
    wtf_size_t index = ctx.scores[previous_index].best_break;
    while (index != kNotFound) {
      item_counts.push_back(previous_index - index);
      previous_index = index;
      index = ctx.scores[index].best_break;
    }
    item_counts.push_back(previous_index + 1);
    DCHECK_EQ(line_count, item_counts.size());
  }

  // Build up the final result, just break based on our pre-computed line-count.
  auto it = item_counts.rbegin();
  auto should_break = [&it](wtf_size_t count, LayoutUnit) {
    if (count == *it) {
      ++it;
      return true;
    }
    return false;
  };
  return BreakIntoLines(all_items, gap_between_items, should_break);
}

FlexLineBreakerResult GreedyBreakFlexItemsIntoLines(
    base::span<FlexItem> all_items,
    const LayoutUnit line_break_size,
    const LayoutUnit gap_between_items,
    const bool is_multi_line) {
  // Greedily break if the current line-size exceeds the break-size.
  auto should_break = [&](wtf_size_t count, LayoutUnit line_size) {
    return is_multi_line && count && line_size > line_break_size;
  };
  return BreakIntoLines(all_items, gap_between_items, should_break);
}

}  // namespace blink
