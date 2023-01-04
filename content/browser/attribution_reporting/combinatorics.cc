// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/combinatorics.h"

#include <functional>

#include "base/check_op.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"

namespace content {

int BinomialCoefficient(int n, int k) {
  DCHECK_GE(n, 0);
  DCHECK_GE(k, 0);

  if (k > n) {
    return 0;
  }

  // Speed up some trivial cases.
  if (k == n || n == 0) {
    return 1;
  }

  // BinomialCoefficient(n, k) == BinomialCoefficient(n, n - k),
  // So simplify if possible.
  if (k > n - k) {
    k = n - k;
  }

  // (n choose k) = n (n -1) ... (n - (k - 1)) / k!
  // = mul((n + i - i) / i), i from 1 -> k.
  //
  // You might be surprised that this algorithm works just fine with integer
  // division (i.e. division occurs cleanly with no remainder). However, this is
  // true for a very simple reason. Imagine a value of `i` causes division with
  // remainder in the below algorithm. This immediately implies that
  // (n choose i) is fractional, which we know is not the case.
  int result = 1;
  for (int i = 1; i <= k; i++) {
    result = base::CheckMul(result, n + 1 - i).ValueOrDie();
    DCHECK_EQ(0, result % i);
    result = result / i;
  }
  return result;
}

// Computes the `combination_index`-th lexicographically smallest k-combination.
// https://en.wikipedia.org/wiki/Combinatorial_number_system

// A k-combination is a sequence of k non-negative integers in decreasing order.
// a_k > a_{k-1} > ... > a_2 > a_1 >= 0.
// k-combinations can be ordered lexicographically, with the smallest
// k-combination being a_k=k-1, a_{k-1}=k-2, .., a_1=0. Given an index
// `combination_index`>=0, and an order k, this method returns the
// `combination_index`-th smallest k-combination.
//
// Given an index `combination_index`, the `combination_index`-th k-combination
// is the unique set of k non-negative integers
// a_k > a_{k-1} > ... > a_2 > a_1 >= 0
// such that `combination_index` = \sum_{i=1}^k {a_i}\choose{i}
//
// We find this set via a simple greedy algorithm.
// http://math0.wvstateu.edu/~baker/cs405/code/Combinadics.html
std::vector<int> GetKCombinationAtIndex(int combination_index, int k) {
  DCHECK_GE(combination_index, 0);
  DCHECK_GE(k, 0);

  std::vector<int> output_k_combination;
  output_k_combination.reserve(k);
  if (k == 0) {
    return output_k_combination;
  }

  // To find a_k, iterate candidates upwards from 0 until we've found the
  // maximum a such that (a choose k) <= `combination_index`. Let a_k = a. Use
  // the previous binomial coefficient to compute the next one. Note: possible
  // to speed this up via something other than incremental search.
  int target = combination_index;
  int candidate = k - 1;
  int binomial_coefficient = 0;       // BinomialCoefficient(candidate, k)
  int next_binomial_coefficient = 1;  // BinomailCoefficient(candidate + 1, k)
  while (next_binomial_coefficient <= target) {
    candidate++;
    binomial_coefficient = next_binomial_coefficient;
    DCHECK_EQ(binomial_coefficient, BinomialCoefficient(candidate, k));

    // (n + 1 choose k) = (n choose k) * (n + 1) / (n + 1 - k)
    next_binomial_coefficient =
        base::CheckMul(binomial_coefficient, candidate + 1).ValueOrDie();
    next_binomial_coefficient /= candidate + 1 - k;
  }
  // We know from the k-combination definition, all subsequent values will be
  // strictly decreasing. Find them all by decrementing `candidate`.
  // Use the previous binomial coefficient to compute the next one.
  int current_k = k;
  while (true) {
    // The optimized code below maintains this loop invariant.
    DCHECK_EQ(binomial_coefficient, BinomialCoefficient(candidate, current_k));
    if (binomial_coefficient <= target) {
      output_k_combination.push_back(candidate);
      target -= binomial_coefficient;
      if (static_cast<int>(output_k_combination.size()) == k) {
        DCHECK_EQ(target, 0);
        return output_k_combination;
      }
      // (n - 1 choose k - 1) = (n choose k) * k / n
      binomial_coefficient = binomial_coefficient * (current_k) / candidate;

      current_k--;
      candidate--;
    } else {
      // (n - 1 choose k) = (n choose k) * (n - k) / n
      binomial_coefficient =
          binomial_coefficient * (candidate - current_k) / candidate;

      candidate--;
    }
  }
}

int GetNumberOfStarsAndBarsSequences(int num_stars, int num_bars) {
  return BinomialCoefficient(num_stars + num_bars, num_stars);
}

std::vector<int> GetStarIndices(int num_stars,
                                int num_bars,
                                int sequence_index) {
  DCHECK_LT(sequence_index,
            GetNumberOfStarsAndBarsSequences(num_stars, num_bars));
  return GetKCombinationAtIndex(sequence_index, num_stars);
}

std::vector<int> GetBarsPrecedingEachStar(std::vector<int> out) {
  DCHECK(base::ranges::is_sorted(out, std::greater{}));

  for (size_t i = 0u; i < out.size(); i++) {
    int star_index = out[i];

    // There are `star_index` prior positions in the sequence, and `i` prior
    // stars, so there are `star_index` - `i` prior bars.
    out[i] = star_index - (out.size() - 1 - i);
  }
  return out;
}

}  // namespace content
