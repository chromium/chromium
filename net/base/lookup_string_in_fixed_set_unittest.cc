// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/lookup_string_in_fixed_set.h"

#include <string.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <ostream>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {
namespace test1 {
#include "net/base/registry_controlled_domains/effective_tld_names_unittest1-inc.cc"
}
namespace test3 {
#include "net/base/registry_controlled_domains/effective_tld_names_unittest3-inc.cc"
}
namespace test4 {
#include "net/base/registry_controlled_domains/effective_tld_names_unittest4-inc.cc"
}
namespace test5 {
#include "net/base/registry_controlled_domains/effective_tld_names_unittest5-inc.cc"
}
namespace test6 {
#include "net/base/registry_controlled_domains/effective_tld_names_unittest6-inc.cc"
}

struct Expectation {
  const char* const key;
  int value;
};

void PrintTo(const Expectation& expectation, std::ostream* os) {
  *os << "{\"" << expectation.key << "\", " << expectation.value << "}";
}

class LookupStringInFixedSetTest : public testing::TestWithParam<Expectation> {
 protected:
  int LookupInGraph(base::span<const uint8_t> graph, const char* key) {
    return LookupStringInFixedSet(graph, key, strlen(key));
  }
};

class Dafsa1Test : public LookupStringInFixedSetTest {};

TEST_P(Dafsa1Test, BasicTest) {
  const Expectation& param = GetParam();
  EXPECT_EQ(param.value, LookupInGraph(test1::kDafsa, param.key));
}

const Expectation kBasicTestCases[] = {
    {"", -1},      {"j", -1},          {"jp", 0}, {"jjp", -1}, {"jpp", -1},
    {"bar.jp", 2}, {"pref.bar.jp", 1}, {"c", 2},  {"b.c", 1},  {"priv.no", 4},
};

// Helper function for EnumerateDafsaLanaguage.
void RecursivelyEnumerateDafsaLanguage(const FixedSetIncrementalLookup& lookup,
                                       std::vector<char>* sequence,
                                       std::vector<std::string>* language) {
  int result = lookup.GetResultForCurrentSequence();
  if (result != kDafsaNotFound) {
    std::string line(sequence->begin(), sequence->end());
    line += base::StringPrintf(", %d", result);
    language->emplace_back(std::move(line));
  }
  // Try appending each char value.
  for (char c = std::numeric_limits<char>::min();; ++c) {
    FixedSetIncrementalLookup continued_lookup = lookup;
    if (continued_lookup.Advance(c)) {
      sequence->push_back(c);
      size_t saved_language_size = language->size();
      RecursivelyEnumerateDafsaLanguage(continued_lookup, sequence, language);
      CHECK_LT(saved_language_size, language->size())
          << "DAFSA includes a branch to nowhere at node: "
          << std::string(sequence->begin(), sequence->end());
      sequence->pop_back();
    }
    if (c == std::numeric_limits<char>::max())
      break;
  }
}

// Uses FixedSetIncrementalLookup to build a vector of every string in the
// language of the DAFSA.
std::vector<std::string> EnumerateDafsaLanguage(
    base::span<const uint8_t> graph) {
  FixedSetIncrementalLookup query(graph);
  std::vector<char> sequence;
  std::vector<std::string> language;
  RecursivelyEnumerateDafsaLanguage(query, &sequence, &language);
  return language;
}

INSTANTIATE_TEST_SUITE_P(LookupStringInFixedSetTest,
                         Dafsa1Test,
                         ::testing::ValuesIn(kBasicTestCases));

class Dafsa3Test : public LookupStringInFixedSetTest {};

// This DAFSA is constructed so that labels begin and end with unique
// characters, which makes it impossible to merge labels. Each inner node
// is about 100 bytes and a one byte offset can at most add 64 bytes to
// previous offset. Thus the paths must go over two byte offsets.
TEST_P(Dafsa3Test, TestDafsaTwoByteOffsets) {
  const Expectation& param = GetParam();
  EXPECT_EQ(param.value, LookupInGraph(test3::kDafsa, param.key));
}

const Expectation kTwoByteOffsetTestCases[] = {
    {"0________________________________________________________________________"
     "____________________________0",
     0},
    {"7________________________________________________________________________"
     "____________________________7",
     4},
    {"a________________________________________________________________________"
     "____________________________8",
     -1},
};

INSTANTIATE_TEST_SUITE_P(LookupStringInFixedSetTest,
                         Dafsa3Test,
                         ::testing::ValuesIn(kTwoByteOffsetTestCases));

class Dafsa4Test : public LookupStringInFixedSetTest {};

// This DAFSA is constructed so that labels begin and end with unique
// characters, which makes it impossible to merge labels. The byte array
// has a size of ~54k. A two byte offset can add at most add 8k to the
// previous offset. Since we can skip only forward in memory, the nodes
// representing the return values must be located near the end of the byte
// array. The probability that we can reach from an arbitrary inner node to
// a return value without using a three byte offset is small (but not zero).
// The test is repeated with some different keys and with a reasonable
// probability at least one of the tested paths has go over a three byte
// offset.
TEST_P(Dafsa4Test, TestDafsaThreeByteOffsets) {
  const Expectation& param = GetParam();
  EXPECT_EQ(param.value, LookupInGraph(test4::kDafsa, param.key));
}

const Expectation kThreeByteOffsetTestCases[] = {
    {"Z6_______________________________________________________________________"
     "_____________________________Z6",
     0},
    {"Z7_______________________________________________________________________"
     "_____________________________Z7",
     4},
    {"Za_______________________________________________________________________"
     "_____________________________Z8",
     -1},
};

INSTANTIATE_TEST_SUITE_P(LookupStringInFixedSetTest,
                         Dafsa4Test,
                         ::testing::ValuesIn(kThreeByteOffsetTestCases));

class Dafsa5Test : public LookupStringInFixedSetTest {};

// This DAFSA is constructed from words with similar prefixes but distinct
// suffixes. The DAFSA will then form a trie with the implicit source node
// as root.
TEST_P(Dafsa5Test, TestDafsaJoinedPrefixes) {
  const Expectation& param = GetParam();
  EXPECT_EQ(param.value, LookupInGraph(test5::kDafsa, param.key));
}

const Expectation kJoinedPrefixesTestCases[] = {
    {"ai", 0},   {"bj", 4},   {"aak", 0},   {"bbl", 4},
    {"aaa", -1}, {"bbb", -1}, {"aaaam", 0}, {"bbbbn", 0},
};

INSTANTIATE_TEST_SUITE_P(LookupStringInFixedSetTest,
                         Dafsa5Test,
                         ::testing::ValuesIn(kJoinedPrefixesTestCases));

class Dafsa6Test : public LookupStringInFixedSetTest {};

// This DAFSA is constructed from words with similar suffixes but distinct
// prefixes. The DAFSA will then form a trie with the implicit sink node as
// root.
TEST_P(Dafsa6Test, TestDafsaJoinedSuffixes) {
  const Expectation& param = GetParam();
  EXPECT_EQ(param.value, LookupInGraph(test6::kDafsa, param.key));
}

const Expectation kJoinedSuffixesTestCases[] = {
    {"ia", 0},   {"jb", 4},   {"kaa", 0},   {"lbb", 4},
    {"aaa", -1}, {"bbb", -1}, {"maaaa", 0}, {"nbbbb", 0},
};

INSTANTIATE_TEST_SUITE_P(LookupStringInFixedSetTest,
                         Dafsa6Test,
                         ::testing::ValuesIn(kJoinedSuffixesTestCases));

// Validates that the generated DAFSA contains exactly the same information as
// effective_tld_names_unittest1.gperf.
TEST(LookupStringInFixedSetTest, Dafsa1EnumerateLanguage) {
  auto language = EnumerateDafsaLanguage(test1::kDafsa);

  // These are the lines of effective_tld_names_unittest1.gperf, in sorted
  // order.
  std::vector<std::string> expected_language = {
      "ac.jp, 0",       "b.c, 1",     "bar.baz.com, 0", "bar.jp, 2",
      "baz.bar.jp, 2",  "c, 2",       "jp, 0",          "no, 0",
      "pref.bar.jp, 1", "priv.no, 4", "private, 4",     "xn--fiqs8s, 0",
  };

  EXPECT_EQ(expected_language, language);
}

// Validates that the generated DAFSA contains exactly the same information as
// effective_tld_names_unittest5.gperf.
TEST(LookupStringInFixedSetTest, Dafsa5EnumerateLanguage) {
  auto language = EnumerateDafsaLanguage(test5::kDafsa);

  std::vector<std::string> expected_language = {
      "aaaam, 0", "aak, 0", "ai, 0", "bbbbn, 0", "bbl, 4", "bj, 4",
  };

  EXPECT_EQ(expected_language, language);
}

// Validates that the generated DAFSA contains exactly the same information as
// effective_tld_names_unittest6.gperf.
TEST(LookupStringInFixedSetTest, Dafsa6EnumerateLanguage) {
  auto language = EnumerateDafsaLanguage(test6::kDafsa);

  std::vector<std::string> expected_language = {
      "ia, 0", "jb, 4", "kaa, 0", "lbb, 4", "maaaa, 0", "nbbbb, 0",
  };

  EXPECT_EQ(expected_language, language);
}

}  // namespace
}  // namespace net
