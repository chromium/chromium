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
  const ScoreUnit gap_between_items;
  ScoreUnit line_break_size;
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

// Using the prefix-sums array returns how many flex-lines would result with
// greedy packing given `line_break_size`.
wtf_size_t GreedyLineCount(const ScoreContext& ctx, ScoreUnit line_break_size) {
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
}

// Applies the `min_line_count` by:
//  - Bisecting the `ctx.line_break_size` to try and achieve the minimum.
//  - Then (if above fails) infinitesimally increasing the size of certain
//    items so they create additional lines.
// Returns the new line-break size.
wtf_size_t ApplyMinLineCount(const wtf_size_t min_line_count,
                             ScoreContext& ctx) {
  // Bisect our `ctx.line_break_size` to try and achieve `min_line_count`.
  {
    // We can clamp `line_break_size` to the maximum size of the items. This is
    // important when we pass `LayoutUnit::Max()` into the line-break for
    // column flexboxes.
    ScoreUnit low = kZero;
    ScoreUnit high =
        std::min(ctx.line_break_size, ctx.sums.back() - ctx.gap_between_items);

    while (low < high) {
      const ScoreUnit midpoint = low + (high - low) / 2;
      if (GreedyLineCount(ctx, midpoint) > min_line_count) {
        low = midpoint + 1u;
      } else {
        high = midpoint;
      }
    }
    ctx.line_break_size = high;
  }

  // Update our line-count based on our new line-break size, and return if
  // we've reached the minimum.
  wtf_size_t line_count = GreedyLineCount(ctx, ctx.line_break_size);
  if (line_count >= min_line_count) {
    return line_count;
  }

  // After running the bisection, we can still be under the minimum. For
  // example: if we have 4 identical items in a 2x2 grid, there is no
  // line-break size which will achieve exactly 3 lines.
  //
  // In order to solve this we "fudge" the sizes of "perfectly fitting" items,
  // e.g. items which are exactly are on the end of a line, so that they shift
  // down one line.
  Vector<wtf_size_t> perfect_fit_indices;
  {
    ScoreUnit previous = kZero;
    bool line_has_content = false;
    for (wtf_size_t i = 0; i < ctx.sums.size(); ++i) {
      const ScoreUnit line_size =
          ctx.sums[i] - previous - ctx.gap_between_items;

      // Check if the current item fits exactly.
      if (line_size == ctx.line_break_size) {
        perfect_fit_indices.push_back(i);
      }

      if (line_size > ctx.line_break_size) {
        previous = line_has_content ? ctx.sums[i - 1] : ctx.sums[i];
        line_has_content = false;
        continue;
      }

      line_has_content = true;
    }
  }

  // Next increase the size of each of our "perfect-fit" items so they get
  // pushed to the next line.
  //
  // Each time we do this we need to check how many lines we have and
  // potentially break (so as we don't create *too many* lines).
  for (wtf_size_t index : base::Reversed(perfect_fit_indices)) {
    for (wtf_size_t i = index; i < ctx.sums.size(); ++i) {
      ++ctx.sums[i];
    }

    // Break if we've reached our desired line-count.
    line_count = GreedyLineCount(ctx, ctx.line_break_size);
    if (line_count >= min_line_count) {
      break;
    }
  }

  return line_count;
}

template <typename ShouldBreakFunc>
FlexLineBreakerResult BreakIntoLines(base::span<FlexItem> all_items,
                                     const LayoutUnit gap_between_items,
                                     const ShouldBreakFunc should_break) {
  HeapVector<InitialFlexLine, 1> flex_lines;
  LayoutUnit max_sum_hypothetical_main_size;

  base::span<FlexItem> items = all_items;

  while (!items.empty()) {
    LayoutUnit sum_hypothetical_main_size;
    wtf_size_t count = 0u;

    for (auto& item : items) {
      if (should_break(count, sum_hypothetical_main_size +
                                  item.HypotheticalMainAxisMarginBoxSize())) {
        break;
      }

      sum_hypothetical_main_size +=
          item.HypotheticalMainAxisMarginBoxSize() + gap_between_items;
      ++count;
    }
    // Take off the last gap (note we *always* have an item in the line).
    sum_hypothetical_main_size -= gap_between_items;

    flex_lines.emplace_back(count, sum_hypothetical_main_size);
    max_sum_hypothetical_main_size =
        std::max(max_sum_hypothetical_main_size, sum_hypothetical_main_size);
    items = items.subspan(count);
  }

  return {flex_lines, max_sum_hypothetical_main_size};
}

}  // namespace

FlexLineBreakerResult BalanceBreakFlexItemsIntoLines(
    base::span<FlexItem> all_items,
    const LayoutUnit line_break_size,
    const LayoutUnit gap_between_items,
    wtf_size_t min_line_count) {
  // We can't have more lines than items.
  min_line_count =
      std::min(min_line_count, ClampTo<wtf_size_t>(all_items.size()));
  DCHECK_GE(min_line_count, 1u);

  ScoreContext ctx = {
      .gap_between_items = static_cast<uint64_t>(gap_between_items.RawValue()),
      .line_break_size = static_cast<uint64_t>(line_break_size.RawValue()),
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

  // Check if We are below the minimum line-count, bisect our line-break size
  // to achieve the minimum number of lines.
  wtf_size_t line_count = GreedyLineCount(ctx, ctx.line_break_size);
  if (line_count < min_line_count) {
    line_count = ApplyMinLineCount(min_line_count, ctx);
  }

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
