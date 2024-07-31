// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef COMPONENTS_ZUCCHINI_SUFFIX_ARRAY_H_
#define COMPONENTS_ZUCCHINI_SUFFIX_ARRAY_H_

#include <algorithm>
#include <iterator>
#include <numeric>
#include <vector>

#include "base/check.h"
#include "base/containers/adapters.h"

namespace zucchini {

// A functor class that implements the naive suffix sorting algorithm that uses
// std::sort with lexicographical compare. This is only meant as reference of
// the interface.
class NaiveSuffixSort {
 public:
  // Type requirements:
  // |InputRng| is an input random access range.
  // |KeyType| is an unsigned integer type.
  // |SAIt| is a random access iterator with mutable references.
  template <class InputRng, class KeyType, class SAIt>
  // |str| is the input string on which suffix sort is applied.
  // Characters found in |str| must be in the range [0, |key_bound|)
  // |suffix_array| is the beginning of the destination range, which is at least
  // as large as |str|.
  void operator()(const InputRng& str,
                  KeyType key_bound,
                  SAIt suffix_array) const {
    using size_type = typename SAIt::value_type;

    size_type n = static_cast<size_type>(std::end(str) - std::begin(str));

    // |suffix_array| is first filled with ordered indices of |str|.
    // Those indices are then sorted with lexicographical comparisons in |str|.
    std::iota(suffix_array, suffix_array + n, 0);
    std::sort(suffix_array, suffix_array + n, [&str](size_type i, size_type j) {
      return std::lexicographical_compare(std::begin(str) + i, std::end(str),
                                          std::begin(str) + j, std::end(str));
    });
  }
};

// A functor class that implements suffix array induced sorting (SA-IS)
// algorithm with linear time and memory complexity,
// see http://ieeexplore.ieee.org/abstract/document/5582081/
class InducedSuffixSort {
 public:
  // Type requirements:
  // |InputRng| is an input random access range.
  // |KeyType| is an unsigned integer type.
  // |SAIt| is a random access iterator with mutable values.
  template <class InputRng, class KeyType, class SAIt>
  // |str| is the input string on which suffix sort is applied.
  // Characters found in |str| must be in the range [0, |key_bound|)
  // |suffix_array| is the beginning of the destination range, which is at least
  // as large as |str|.
  void operator()(const InputRng& str,
                  KeyType key_bound,
                  SAIt suffix_array) const {
    using value_type = typename InputRng::value_type;
    using size_type = typename SAIt::value_type;

    static_assert(std::is_unsigned<value_type>::value,
                  "SA-IS only supports input string with unsigned values");
    static_assert(std::is_unsigned<KeyType>::value, "KeyType must be unsigned");

    size_type n = static_cast<size_type>(std::end(str) - std::begin(str));

    Implementation<size_type, KeyType>::SuffixSort(std::begin(str), n,
                                                   key_bound, suffix_array);
  }

  // Given string S of length n. We assume S is terminated by a unique sentinel
  // $, which is considered as the smallest character. This sentinel does not
  // exist in memory and is only treated implicitly, hence |n| does not count
  // the sentinel in this implementation. We denote suf(S,i) the suffix formed
  // by S[i..n).

  // A suffix suf(S,i) is said to be S-type or L-type, if suf(S,i) < suf(S,i+1)
  // or suf(S,i) > suf(S,i+1), respectively.
  enum SLType : bool { SType, LType };

  // A character S[i] is said to be S-type or L-type if the suffix suf(S,i) is
  // S-type or L-type, respectively.

  // A character S[i] is called LMS (leftmost S-type), if S[i] is S-type and
  // S[i-1] is L-type. A suffix suf(S,i) is called LMS, if S[i] is an LMS
  // character.

  // A substring S[i..j) is an LMS-substring if
  // (1) S[i] is LMS, S[j] is LMS or the sentinel $, and S[i..j) has no other
  //     LMS characters, or
  // (2) S[i..j) is the sentinel $.

  template <class SizeType, class KeyType>
  struct Implementation {
    static_assert(std::is_unsigned<SizeType>::value,
                  "SizeType must be unsigned");
    static_assert(std::is_unsigned<KeyType>::value, "KeyType must be unsigned");
    using size_type = SizeType;
    using key_type = KeyType;

    using iterator = typename std::vector<size_type>::iterator;
    using const_iterator = typename std::vector<size_type>::const_iterator;

    // Partition every suffix based on SL-type. Returns the number of LMS
    // suffixes.
    template <class StrIt>
    static size_type BuildSLPartition(
        StrIt str,
        size_type length,
        key_type key_bound,
        std::vector<SLType>::reverse_iterator sl_partition_it) {
      // We will count LMS suffixes (S to L-type or last S-type).
      size_type lms_count = 0;

      // |previous_type| is initialized to L-type to avoid counting an extra
      // LMS suffix at the end
      SLType previous_type = LType;

      // Initialized to dummy, impossible key.
      key_type previous_key = key_bound;

      // We're travelling backward to determine the partition,
      // as if we prepend one character at a time to the string, ex:
      // b$ is L-type because b > $.
      // ab$ is S-type because a < b, implying ab$ < b$.
      // bab$ is L-type because b > a, implying bab$ > ab$.
      // bbab$ is L-type, because bab$ was also L-type, implying bbab$ > bab$.
      for (auto str_it = std::reverse_iterator<StrIt>(str + length);
           str_it != std::reverse_iterator<StrIt>(str);
           ++str_it, ++sl_partition_it) {
        key_type current_key = *str_it;

        if (current_key > previous_key || previous_key == key_bound) {
          // S[i] > S[i + 1] or S[i] is last character.
          if (previous_type == SType)
            // suf(S,i) is L-type and suf(S,i + 1) is S-type, therefore,
            // suf(S,i+1) was a LMS suffix.
            ++lms_count;

          previous_type = LType;  // For next round.
        } else if (current_key < previous_key) {
          // S[i] < S[i + 1]
          previous_type = SType;  // For next round.
        }
        // Else, S[i] == S[i + 1]:
        // The next character that differs determines the SL-type,
        // so we reuse the last seen type.

        *sl_partition_it = previous_type;
        previous_key = current_key;  // For next round.
      }

      return lms_count;
    }

    // Find indices of LMS suffixes and write result to |lms_indices|.
    static void FindLmsSuffixes(const std::vector<SLType>& sl_partition,
                                iterator lms_indices) {
      // |previous_type| is initialized to S-type to avoid counting an extra
      // LMS suffix at the beginning
      SLType previous_type = SType;
      for (size_type i = 0; i < sl_partition.size(); ++i) {
        if (sl_partition[i] == SType && previous_type == LType)
          *lms_indices++ = i;
        previous_type = sl_partition[i];
      }
    }

    template <class StrIt>
    static std::vector<size_type> MakeBucketCount(StrIt str,
                                                  size_type length,
                                                  key_type key_bound) {
      // Occurrence of every unique character is counted in |buckets|
      std::vector<size_type> buckets(static_cast<size_type>(key_bound));

      for (auto it = str; it != str + length; ++it)
        ++buckets[*it];
      return buckets;
    }

    // Apply induced sort from |lms_indices| to |suffix_array| associated with
    // the string |str|.
    template <class StrIt, class SAIt>
    static void InducedSort(StrIt str,
                            size_type length,
                            const std::vector<SLType>& sl_partition,
                            const std::vector<size_type>& lms_indices,
                            const std::vector<size_type>& buckets,
                            SAIt suffix_array) {
      // All indices are first marked as unset with the illegal value |length|.
      std::fill(suffix_array, suffix_array + length, length);

      // Used to mark bucket boundaries (head or end) as indices in str.
      DCHECK(!buckets.empty());
      std::vector<size_type> bucket_bounds(buckets.size());

      // Step 1: Assign indices for LMS suffixes, populating the end of
      // respective buckets but keeping relative order.

      // Find the end of each bucket and write it to |bucket_bounds|.
      std::partial_sum(buckets.begin(), buckets.end(), bucket_bounds.begin());

      // Process each |lms_indices| backward, and assign them to the end of
      // their respective buckets, so relative order is preserved.
      for (size_t lms_index : base::Reversed(lms_indices)) {
        key_type key = str[lms_index];
        suffix_array[--bucket_bounds[key]] = lms_index;
      }

      // Step 2
      // Scan forward |suffix_array|; for each modified suf(S,i) for which
      // suf(S,SA(i) - 1) is L-type, place suf(S,SA(i) - 1) to the current
      // head of the corresponding bucket and forward the bucket head to the
      // right.

      // Find the head of each bucket and write it to |bucket_bounds|. Since
      // only LMS suffixes where inserted in |suffix_array| during Step 1,
      // |bucket_bounds| does not contains the head of each bucket and needs to
      // be updated.
      bucket_bounds[0] = 0;
      std::partial_sum(buckets.begin(), buckets.end() - 1,
                       bucket_bounds.begin() + 1);

      // From Step 1, the sentinel $, which we treat implicitly, would have
      // been placed at the beginning of |suffix_array|, since $ is always
      // considered as the smallest character. We then have to deal with the
      // previous (last) suffix.
      if (sl_partition[length - 1] == LType) {
        key_type key = str[length - 1];
        suffix_array[bucket_bounds[key]++] = length - 1;
      }
      for (auto it = suffix_array; it != suffix_array + length; ++it) {
        size_type suffix_index = *it;

        // While the original algorithm marks unset suffixes with -1,
        // we found that marking them with |length| is also possible and more
        // convenient because we are working with unsigned integers.
        if (suffix_index != length && suffix_index > 0 &&
            sl_partition[--suffix_index] == LType) {
          key_type key = str[suffix_index];
          suffix_array[bucket_bounds[key]++] = suffix_index;
        }
      }

      // Step 3
      // Scan backward |suffix_array|; for each modified suf(S, i) for which
      // suf(S,SA(i) - 1) is S-type, place suf(S,SA(i) - 1) to the current
      // end of the corresponding bucket and forward the bucket head to the
      // left.

      // Find the end of each bucket and write it to |bucket_bounds|. Since
      // only L-type suffixes where inserted in |suffix_array| during Step 2,
      // |bucket_bounds| does not contain the end of each bucket and needs to
      // be updated.
      std::partial_sum(buckets.begin(), buckets.end(), bucket_bounds.begin());

      for (auto it = std::reverse_iterator<SAIt>(suffix_array + length);
           it != std::reverse_iterator<SAIt>(suffix_array); ++it) {
        size_type suffix_index = *it;
        if (suffix_index != length && suffix_index > 0 &&
            sl_partition[--suffix_index] == SType) {
          key_type key = str[suffix_index];
          suffix_array[--bucket_bounds[key]] = suffix_index;
        }
      }
      // Deals with the last suffix, because of the sentinel.
      if (sl_partition[length - 1] == SType) {
        key_type key = str[length - 1];
        suffix_array[--bucket_bounds[key]] = length - 1;
      }
    }

    // Given a string S starting at |str| with length |length|, an array
    // starting at |substring_array| containing lexicographically ordered LMS
    // terminated substring indices of S and an SL-Type partition |sl_partition|
    // of S, assigns a unique label to every unique LMS substring. The sorted
    // labels for all LMS substrings are written to |lms_str|, while the indices
    // of LMS suffixes are written to |lms_indices|. In addition, returns the
    // total number of unique labels.
    template <class StrIt, class SAIt>
    static size_type LabelLmsSubstrings(StrIt str,
                                        size_type length,
                                        const std::vector<SLType>& sl_partition,
                                        SAIt suffix_array,
                                        iterator lms_indices,
                                        iterator lms_str) {
      // Labelling starts at 0.
      size_type label = 0;

      // |previous_lms| is initialized to 0 to indicate it is unset.
      // Note that suf(S,0) is never a LMS suffix. Substrings will be visited in
      // lexicographical order.
      size_type previous_lms = 0;
      for (auto it = suffix_array; it != suffix_array + length; ++it) {
        if (*it > 0 && sl_partition[*it] == SType &&
            sl_partition[*it - 1] == LType) {
          // suf(S, *it) is a LMS suffix.

          size_type current_lms = *it;
          if (previous_lms != 0) {
            // There was a previous LMS suffix. Check if the current LMS
            // substring is equal to the previous one.
            SLType current_lms_type = SType;
            SLType previous_lms_type = SType;
            for (size_type k = 0;; ++k) {
              // |current_lms_end| and |previous_lms_end| denote whether we have
              // reached the end of the current and previous LMS substring,
              // respectively
              bool current_lms_end = false;
              bool previous_lms_end = false;

              // Check for both previous and current substring ends.
              // Note that it is more convenient to check if
              // suf(S,current_lms + k) is an LMS suffix than to retrieve it
              // from lms_indices.
              if (current_lms + k >= length ||
                  (current_lms_type == LType &&
                   sl_partition[current_lms + k] == SType)) {
                current_lms_end = true;
              }
              if (previous_lms + k >= length ||
                  (previous_lms_type == LType &&
                   sl_partition[previous_lms + k] == SType)) {
                previous_lms_end = true;
              }

              if (current_lms_end && previous_lms_end) {
                break;  // Previous and current substrings are identical.
              } else if (current_lms_end != previous_lms_end ||
                         str[current_lms + k] != str[previous_lms + k]) {
                // Previous and current substrings differ, a new label is used.
                ++label;
                break;
              }

              current_lms_type = sl_partition[current_lms + k];
              previous_lms_type = sl_partition[previous_lms + k];
            }
          }
          *lms_indices++ = *it;
          *lms_str++ = label;
          previous_lms = current_lms;
        }
      }

      return label + 1;
    }

    // Implementation of the SA-IS algorithm. |str| must be a random access
    // iterator pointing at the beginning of S with length |length|. The result
    // is writtend in |suffix_array|, a random access iterator.
    template <class StrIt, class SAIt>
    static void SuffixSort(StrIt str,
                           size_type length,
                           key_type key_bound,
                           SAIt suffix_array) {
      if (length == 1)
        *suffix_array = 0;
      if (length < 2)
        return;

      std::vector<SLType> sl_partition(length);
      size_type lms_count =
          BuildSLPartition(str, length, key_bound, sl_partition.rbegin());
      std::vector<size_type> lms_indices(lms_count);
      FindLmsSuffixes(sl_partition, lms_indices.begin());
      std::vector<size_type> buckets = MakeBucketCount(str, length, key_bound);

      if (lms_indices.size() > 1) {
        // Given |lms_indices| in the same order they appear in |str|, induce
        // LMS substrings relative order and write result to |suffix_array|.
        InducedSort(str, length, sl_partition, lms_indices, buckets,
                    suffix_array);
        std::vector<size_type> lms_str(lms_indices.size());

        // Given LMS substrings in relative order found in |suffix_array|,
        // map LMS substrings to unique labels to form a new string, |lms_str|.
        size_type label_count =
            LabelLmsSubstrings(str, length, sl_partition, suffix_array,
                               lms_indices.begin(), lms_str.begin());

        if (label_count < lms_str.size()) {
          // Reorder |lms_str| to have LMS suffixes in the same order they
          // appear in |str|.
          for (size_type i = 0; i < lms_indices.size(); ++i)
            suffix_array[lms_indices[i]] = lms_str[i];

          SLType previous_type = SType;
          for (size_type i = 0, j = 0; i < sl_partition.size(); ++i) {
            if (sl_partition[i] == SType && previous_type == LType) {
              lms_str[j] = suffix_array[i];
              lms_indices[j++] = i;
            }
            previous_type = sl_partition[i];
          }

          // Recursively apply SuffixSort on |lms_str|, which is formed from
          // labeled LMS suffixes in the same order they appear in |str|.
          // Note that |KeyType| will be size_type because |lms_str| contains
          // indices. |lms_str| is at most half the length of |str|.
          Implementation<size_type, size_type>::SuffixSort(
              lms_str.begin(), static_cast<size_type>(lms_str.size()),
              label_count, suffix_array);

          // Map LMS labels back to indices in |str| and write result to
          // |lms_indices|. We're using |suffix_array| as a temporary buffer.
          for (size_type i = 0; i < lms_indices.size(); ++i)
            suffix_array[i] = lms_indices[suffix_array[i]];
          std::copy_n(suffix_array, lms_indices.size(), lms_indices.begin());

          // At this point, |lms_indices| contains sorted LMS suffixes of |str|.
        }
      }
      // Given |lms_indices| where LMS suffixes are sorted, induce the full
      // order of suffixes in |str|.
      InducedSort(str, length, sl_partition, lms_indices, buckets,
                  suffix_array);
    }

    Implementation() = delete;
    Implementation(const Implementation&) = delete;
    const Implementation& operator=(const Implementation&) = delete;
  };
};

// Generates a sorted suffix array for the input string |str| using the functor
// |Algorithm| which provides an interface equivalent to NaiveSuffixSort.
/// Characters found in |str| are assumed to be in range [0, |key_bound|).
// Returns the suffix array as a vector.
// |StrRng| is an input random access range.
// |KeyType| is an unsigned integer type.
template <class Algorithm, class StrRng, class KeyType>
std::vector<typename StrRng::size_type> MakeSuffixArray(const StrRng& str,
                                                        KeyType key_bound) {
  Algorithm sort;
  std::vector<typename StrRng::size_type> suffix_array(str.end() - str.begin());
  sort(str, key_bound, suffix_array.begin());
  return suffix_array;
}

// Type requirements:
// |SARng| is an input random access range.
// |StrIt1| is a random access iterator.
// |StrIt2| is a forward iterator.
template <class SARng, class StrIt1, class StrIt2>
// Lexicographical lower bound using binary search for
// [|str2_first|, |str2_last|) in the suffix array |suffix_array| of a string
// starting at |str1_first|. This does not necessarily return the index of
// the longest matching substring.
auto SuffixLowerBound(const SARng& suffix_array,
                      StrIt1 str1_first,
                      StrIt2 str2_first,
                      StrIt2 str2_last) -> decltype(std::begin(suffix_array)) {
  using size_type = typename SARng::value_type;

  size_t n = std::end(suffix_array) - std::begin(suffix_array);
  auto it = std::lower_bound(
      std::begin(suffix_array), std::end(suffix_array), str2_first,
      [str1_first, str2_last, n](size_type a, StrIt2 b) {
        return std::lexicographical_compare(str1_first + a, str1_first + n, b,
                                            str2_last);
      });
  return it;
}

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_SUFFIX_ARRAY_H_
