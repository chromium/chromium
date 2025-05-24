// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/pickle.h"

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/pickle.h"
#include "net/base/pickle_traits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace net {

namespace {

using ::testing::Optional;

// Tests that base::Pickle methods and WriteToPickle() interoperate. There's no
// clean way to avoid the boilerplate, so just use macros.
#define PICKLE_READ_WRITE_COMPATIBLE_TEST(PickleTypeName, value)    \
  TEST(PickleTest, PickleReadCompatible##PickleTypeName) {          \
    base::Pickle pickle;                                            \
    pickle.Write##PickleTypeName(value);                            \
    using Value = decltype(value);                                  \
    base::PickleIterator iter(pickle);                              \
    EXPECT_THAT(ReadValueFromPickle<Value>(iter), Optional(value)); \
  }                                                                 \
                                                                    \
  TEST(PickleTest, PickleWriteCompatible##PickleTypeName) {         \
    base::Pickle pickle1;                                           \
    pickle1.Write##PickleTypeName(value);                           \
    base::Pickle pickle2;                                           \
    WriteToPickle(pickle2, value);                                  \
    EXPECT_EQ(base::span(pickle1), base::span(pickle2));            \
  }                                                                 \
  static_assert(true, "eat colon")

PICKLE_READ_WRITE_COMPATIBLE_TEST(Bool, true);
PICKLE_READ_WRITE_COMPATIBLE_TEST(Int, 123);
// Don't test Long. No-one should be using Long.
PICKLE_READ_WRITE_COMPATIBLE_TEST(UInt16, uint16_t{123});
PICKLE_READ_WRITE_COMPATIBLE_TEST(UInt32, uint32_t{0xFFFFFFFF});
PICKLE_READ_WRITE_COMPATIBLE_TEST(Int64, int64_t{-42});
PICKLE_READ_WRITE_COMPATIBLE_TEST(UInt64, uint64_t{77});
// Float and Double are not supported by WriteToPickle().

PICKLE_READ_WRITE_COMPATIBLE_TEST(String, std::string("walla"));

// Generic implementation of a test that converts a value to a pickle and back
// again and checks the result matches the original.
template <typename T>
void PerformRoundTripTest(const T& value) {
  SCOPED_TRACE(PRETTY_FUNCTION);
  base::Pickle pickle;
  WriteToPickle(pickle, value);
  EXPECT_THAT(ReadValueFromPickle<T>(pickle), Optional(value));
}

// Returns a large set of test values of different types to be used in the
// following tests. These are stored in a tuple as a convenient heterogenous
// container.
auto TestData() {
  return std::make_tuple(
      true, false, 123, 0xFFFFFFFF, -42, uint8_t{0}, std::string("five"),
      std::vector<int>{1, 2, 3}, std::vector<std::string>{"foo", "bar"},
      std::optional<int>(42), std::optional<std::string>("hello"),
      std::make_pair(1, 'a'), std::make_tuple(1, 'a', 2),
      std::map<std::string, std::list<int16_t>>{
          {"foo", {1, 2, 3}}, {"bar", {}}, {"", {0x7fff}}},
      base::flat_set<uint64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
      base::flat_map<std::string, std::vector<std::string>>{
          {"foo", {"bar", "baz"}}, {"", {"qux", "quux"}}},
      absl::InlinedVector<char, 5>{'a', 'c'}, std::vector<uint8_t>{1, 2, 3},
      std::deque<int16_t>{1, -1}, std::vector<bool>{true, false, true},
      absl::InlinedVector<int8_t, 256>{1, 2, 3, 4},
      std::set<int64_t>{81, 12, 17}, std::unordered_set<int64_t>{9, 3, 6},
      std::unordered_map<int, std::string>{{1, "foo"}, {2, "bar"}, {3, "baz"}},
      std::multiset<uint32_t>{1, 1, 1, 2, 2},
      std::multimap<std::string, int>{{"foo", 1}, {"foo", 2}, {"foot", 3}},
      std::unordered_multiset<int>{{1, 2, 3, 4, 5, 6, 1, 2, 3}},
      std::unordered_multimap<int, int>{{1, 2}, {1, 3}},
      std::pair<const int, int>(1, 2), std::optional<const int>(3),
      std::map<char, const std::string>{{'f', "foo"}, {'b', "bar"}},
      std::u16string(u"nicode"));
}

template <typename Functor, size_t... I>
void ForEachTestDataImpl(Functor f, std::index_sequence<I...>) {
  const auto data = TestData();
  (f(std::get<I>(data)), ...);
}

// Calls f(value) for each value in TestData().
template <typename Functor>
void ForEachTestData(Functor f) {
  ForEachTestDataImpl(
      f, std::make_index_sequence<std::tuple_size_v<decltype(TestData())>>());
}

TEST(PickleTest, RoundTrip) {
  ForEachTestData([](const auto& value) { PerformRoundTripTest(value); });
}

TEST(PickleTest, ReadValuesFromPickle) {
  base::Pickle pickle;
  WriteToPickle(pickle, 1, std::string("foo"), 'a');
  auto result = ReadValuesFromPickle<int, std::string, char>(pickle);
  EXPECT_THAT(result, Optional(std::make_tuple(1, "foo", 'a')));
}

TEST(PickleTest, RoundTripTuple) {
  base::Pickle pickle;
  WriteToPickle(pickle, TestData());
  EXPECT_THAT(ReadValueFromPickle<decltype(TestData())>(pickle),
              Optional(TestData()));
}

TEST(PickleTest, RejectEmptyPickle) {
  ForEachTestData([](const auto& value) {
    SCOPED_TRACE(PRETTY_FUNCTION);
    base::Pickle pickle;
    EXPECT_FALSE(
        ReadValueFromPickle<std::remove_cvref_t<decltype(value)>>(pickle));
  });
}

TEST(PickleTest, RejectPickleWithTooLittleData) {
  ForEachTestData([](const auto& value) {
    // Skip cases that might have enough data. base::Pickle aligns all
    // allocations to 4 bytes, so the smallest non-empty base::Pickle is 4 bytes
    // in size.
    if (sizeof(value) <= 4) {
      return;
    }
    SCOPED_TRACE(PRETTY_FUNCTION);
    base::Pickle pickle;
    WriteToPickle(pickle, 'a');
    EXPECT_FALSE(
        ReadValueFromPickle<std::remove_cvref_t<decltype(value)>>(pickle));
  });
}

TEST(PickleTest, EstimatePickleSize) {
  ForEachTestData([](const auto& value) {
    SCOPED_TRACE(PRETTY_FUNCTION);
    base::Pickle pickle;
    WriteToPickle(pickle, value);
    // EstimatePickleSize() is not guaranteed to be exact in general, but it
    // happens to be correct for the cases were are testing.
    EXPECT_EQ(pickle.payload_size(), EstimatePickleSize(value));
  });
}

TEST(PickleTest, EstimatePickleSizeMultipleValues) {
  int i = 3;
  std::string s = "word";
  bool b = true;
  EXPECT_EQ(
      EstimatePickleSize(i, s, b),
      EstimatePickleSize(i) + EstimatePickleSize(s) + EstimatePickleSize(b));
}

// Fuzz test. To cover all the cases, we need to include these types:
// * int (for kCopyAsBytes codepath)
// * std::string (for IsConstructableFromCharLikeIteratorPair path)
// * std::vector<uint16_t> (for CanResizeAndCopyFromBytes path)
// * std::vector<T> for some T that is not kCopyAsBytes, for the
//   IsReserveAndPushBackable path
// * std::set for the IsInsertAndEnable path.
// * std::tuple for the CanSerializeDeserializeTuple path,
// * bool for the bool specialization,
// * std::optional for the std::optional specialization.
using FuzzType = std::set<std::tuple<bool,
                                     std::optional<int>,
                                     std::vector<std::string>,
                                     std::vector<uint16_t>>>;

void FuzzRoundTrip(const FuzzType& value) {
  base::Pickle pickle;
  WriteToPickle(pickle, value);
  EXPECT_EQ(ReadValueFromPickle<FuzzType>(pickle), value);
}

FUZZ_TEST(PickleTest, FuzzRoundTrip);

// Tests for user-defined serialization.

class MyHostPortPair {
 public:
  MyHostPortPair(std::string_view host, uint16_t port)
      : host_(host), port_(port) {}

  const std::string& host() const { return host_; }
  uint16_t port() const { return port_; }

  bool operator==(const MyHostPortPair& other) const = default;

 private:
  std::string host_;
  uint16_t port_;
};

class MyHeaders {
 public:
  MyHeaders() = default;

  void Add(std::string_view name, std::string_view value) {
    headers_.emplace_back(name, value);
  }

  bool operator==(const MyHeaders& other) const = default;

 private:
  friend struct PickleTraits<MyHeaders>;

  std::vector<std::pair<std::string, std::string>> headers_;
};

struct MyHttpVersion {
  uint16_t major;
  uint16_t minor;

  bool operator==(const MyHttpVersion& other) const = default;
};

}  // namespace

// We cannot specialize PickleTraits from inside the anonymous namespace, so we
// had to leave it.

template <>
struct PickleTraits<MyHostPortPair> {
  static void Serialize(base::Pickle& pickle, const MyHostPortPair& value) {
    WriteToPickle(pickle, value.host(), value.port());
  }

  static std::optional<MyHostPortPair> Deserialize(base::PickleIterator& iter) {
    auto result = ReadValuesFromPickle<std::string, uint16_t>(iter);
    if (!result) {
      return std::nullopt;
    }
    auto [host, port] = std::move(result).value();
    return MyHostPortPair(host, port);
  }
};

template <>
struct PickleTraits<MyHeaders> {
  static void Serialize(base::Pickle& pickle, const MyHeaders& value) {
    WriteToPickle(pickle, value.headers_);
  }

  static std::optional<MyHeaders> Deserialize(base::PickleIterator& iter) {
    MyHeaders headers;
    if (!ReadPickleInto(iter, headers.headers_)) {
      return std::nullopt;
    }

    return headers;
  }

  static size_t PickleSize(const MyHeaders& value) {
    return EstimatePickleSize(value.headers_);
  }
};

template <>
constexpr bool kPickleAsBytes<MyHttpVersion> = true;

namespace {

TEST(PickleTraitsTest, MyHostPortPair) {
  MyHostPortPair pair("foo", 42);
  PerformRoundTripTest(pair);
}

TEST(PickleTraitsTest, MyHeaders) {
  MyHeaders headers;
  headers.Add("User-Agent", "Mozilla/5.0");
  headers.Add("Content-Type", "application/octet-stream");
  PerformRoundTripTest(headers);
}

TEST(PickleTraitsTest, MyHttpVersion) {
  MyHttpVersion version = {1, 2};
  PerformRoundTripTest(version);
}

}  // namespace
}  // namespace net
