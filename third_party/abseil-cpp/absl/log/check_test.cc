//
// Copyright 2022 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/log/check.h"

#include <ostream>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/log/internal/test_helpers.h"

namespace {
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Not;

auto* test_env ABSL_ATTRIBUTE_UNUSED = ::testing::AddGlobalTestEnvironment(
    new absl::log_internal::LogTestEnvironment);

#if GTEST_HAS_DEATH_TEST

TEST(CHECKDeathTest, TestBasicValues) {
  CHECK(true);

  EXPECT_DEATH(CHECK(false), "Check failed: false");

  int i = 2;
  CHECK(i != 3);  // NOLINT
}

#endif  // GTEST_HAS_DEATH_TEST

TEST(CHECKTest, TestLogicExpressions) {
  int i = 5;
  CHECK(i > 0 && i < 10);
  CHECK(i < 0 || i > 3);
}

#if ABSL_INTERNAL_CPLUSPLUS_LANG >= 201703L
ABSL_CONST_INIT const auto global_var_check = [](int i) {
  CHECK(i > 0);  // NOLINT
  return i + 1;
}(3);

ABSL_CONST_INIT const auto global_var = [](int i) {
  CHECK_GE(i, 0);  // NOLINT
  return i + 1;
}(global_var_check);
#endif  // ABSL_INTERNAL_CPLUSPLUS_LANG

TEST(CHECKTest, TestPlacementsInCompoundStatements) {
  // check placement inside if/else clauses
  if (true) CHECK(true);

  if (false)
    ;  // NOLINT
  else
    CHECK(true);

  switch (0)
  case 0:
    CHECK(true);  // NOLINT

#if ABSL_INTERNAL_CPLUSPLUS_LANG >= 201703L
  constexpr auto var = [](int i) {
    CHECK(i > 0);  // NOLINT
    return i + 1;
  }(global_var);
  (void)var;
#endif  // ABSL_INTERNAL_CPLUSPLUS_LANG
}

TEST(CHECKTest, TestBoolConvertible) {
  struct Tester {
  } tester;
  CHECK([&]() { return &tester; }());
}

#if GTEST_HAS_DEATH_TEST

TEST(CHECKDeathTest, TestChecksWithSideeffects) {
  int var = 0;
  CHECK([&var]() {
    ++var;
    return true;
  }());
  EXPECT_EQ(var, 1);

  EXPECT_DEATH(CHECK([&var]() {
                 ++var;
                 return false;
               }()) << var,
               "Check failed: .* 2");
}

#endif  // GTEST_HAS_DEATH_TEST

#if GTEST_HAS_DEATH_TEST

TEST(CHECKDeachTest, TestOrderOfInvocationsBetweenCheckAndMessage) {
  int counter = 0;

  auto GetStr = [&counter]() -> std::string {
    return counter++ == 0 ? "" : "non-empty";
  };

  EXPECT_DEATH(CHECK(!GetStr().empty()) << GetStr(), HasSubstr("non-empty"));
}

TEST(CHECKTest, TestSecondaryFailure) {
  auto FailingRoutine = []() {
    CHECK(false) << "Secondary";
    return false;
  };
  EXPECT_DEATH(CHECK(FailingRoutine()) << "Primary",
               AllOf(HasSubstr("Secondary"), Not(HasSubstr("Primary"))));
}

TEST(CHECKTest, TestSecondaryFailureInMessage) {
  auto MessageGen = []() {
    CHECK(false) << "Secondary";
    return "Primary";
  };
  EXPECT_DEATH(CHECK(false) << MessageGen(),
               AllOf(HasSubstr("Secondary"), Not(HasSubstr("Primary"))));
}

#endif  // GTEST_HAS_DEATH_TEST

TEST(CHECKTest, TestBinaryChecksWithPrimitives) {
  CHECK_EQ(1, 1);
  CHECK_NE(1, 2);
  CHECK_GE(1, 1);
  CHECK_GE(2, 1);
  CHECK_LE(1, 1);
  CHECK_LE(1, 2);
  CHECK_GT(2, 1);
  CHECK_LT(1, 2);
}

// For testing using CHECK*() on anonymous enums.
enum { CASE_A, CASE_B };

TEST(CHECKTest, TestBinaryChecksWithEnumValues) {
  // Tests using CHECK*() on anonymous enums.
  CHECK_EQ(CASE_A, CASE_A);
  CHECK_NE(CASE_A, CASE_B);
  CHECK_GE(CASE_A, CASE_A);
  CHECK_GE(CASE_B, CASE_A);
  CHECK_LE(CASE_A, CASE_A);
  CHECK_LE(CASE_A, CASE_B);
  CHECK_GT(CASE_B, CASE_A);
  CHECK_LT(CASE_A, CASE_B);
}

TEST(CHECKTest, TestBinaryChecksWithNullptr) {
  const void* p_null = nullptr;
  const void* p_not_null = &p_null;
  CHECK_EQ(p_null, nullptr);
  CHECK_EQ(nullptr, p_null);
  CHECK_NE(p_not_null, nullptr);
  CHECK_NE(nullptr, p_not_null);
}

#if GTEST_HAS_DEATH_TEST

// Test logging of various char-typed values by failing CHECK*().
TEST(CHECKDeathTest, TestComparingCharsValues) {
  {
    char a = ';';
    char b = 'b';
    EXPECT_DEATH(CHECK_EQ(a, b), "Check failed: a == b \\(';' vs. 'b'\\)");
    b = 1;
    EXPECT_DEATH(CHECK_EQ(a, b),
                 "Check failed: a == b \\(';' vs. char value 1\\)");
  }
  {
    signed char a = ';';
    signed char b = 'b';
    EXPECT_DEATH(CHECK_EQ(a, b), "Check failed: a == b \\(';' vs. 'b'\\)");
    b = -128;
    EXPECT_DEATH(CHECK_EQ(a, b),
                 "Check failed: a == b \\(';' vs. signed char value -128\\)");
  }
  {
    unsigned char a = ';';
    unsigned char b = 'b';
    EXPECT_DEATH(CHECK_EQ(a, b), "Check failed: a == b \\(';' vs. 'b'\\)");
    b = 128;
    EXPECT_DEATH(CHECK_EQ(a, b),
                 "Check failed: a == b \\(';' vs. unsigned char value 128\\)");
  }
}

TEST(CHECKDeathTest, TestNullValuesAreReportedCleanly) {
  const char* a = nullptr;
  const char* b = nullptr;
  EXPECT_DEATH(CHECK_NE(a, b),
               "Check failed: a != b \\(\\(null\\) vs. \\(null\\)\\)");

  a = "xx";
  EXPECT_DEATH(CHECK_EQ(a, b), "Check failed: a == b \\(xx vs. \\(null\\)\\)");
  EXPECT_DEATH(CHECK_EQ(b, a), "Check failed: b == a \\(\\(null\\) vs. xx\\)");

  std::nullptr_t n{};
  EXPECT_DEATH(CHECK_NE(n, nullptr),
               "Check failed: n != nullptr \\(\\(null\\) vs. \\(null\\)\\)");
}

#endif  // GTEST_HAS_DEATH_TEST

TEST(CHECKTest, TestSTREQ) {
  CHECK_STREQ("this", "this");
  CHECK_STREQ(nullptr, nullptr);
  CHECK_STRCASEEQ("this", "tHiS");
  CHECK_STRCASEEQ(nullptr, nullptr);
  CHECK_STRNE("this", "tHiS");
  CHECK_STRNE("this", nullptr);
  CHECK_STRCASENE("this", "that");
  CHECK_STRCASENE(nullptr, "that");
  CHECK_STREQ((std::string("a") + "b").c_str(), "ab");
  CHECK_STREQ(std::string("test").c_str(),
              (std::string("te") + std::string("st")).c_str());
}

TEST(CHECKTest, TestComparisonPlacementsInCompoundStatements) {
  // check placement inside if/else clauses
  if (true) CHECK_EQ(1, 1);
  if (true) CHECK_STREQ("c", "c");

  if (false)
    ;  // NOLINT
  else
    CHECK_LE(0, 1);

  if (false)
    ;  // NOLINT
  else
    CHECK_STRNE("a", "b");

  switch (0)
  case 0:
    CHECK_NE(1, 0);

  switch (0)
  case 0:
    CHECK_STRCASEEQ("A", "a");

#if ABSL_INTERNAL_CPLUSPLUS_LANG >= 201703L
  constexpr auto var = [](int i) {
    CHECK_GT(i, 0);
    return i + 1;
  }(global_var);
  (void)var;

  // CHECK_STR... checks are not supported in constexpr routines.
  // constexpr auto var2 = [](int i) {
  //  CHECK_STRNE("c", "d");
  //  return i + 1;
  // }(global_var);

#if defined(__GNUC__)
  int var3 = (({ CHECK_LE(1, 2); }), global_var < 10) ? 1 : 0;
  (void)var3;

  int var4 = (({ CHECK_STREQ("a", "a"); }), global_var < 10) ? 1 : 0;
  (void)var4;
#endif  // __GNUC__
#endif  // ABSL_INTERNAL_CPLUSPLUS_LANG
}

TEST(CHECKTest, TestDCHECK) {
#ifdef NDEBUG
  DCHECK(1 == 2) << " DCHECK's shouldn't be compiled in normal mode";
#endif
  DCHECK(1 == 1);  // NOLINT(readability/check)
  DCHECK_EQ(1, 1);
  DCHECK_NE(1, 2);
  DCHECK_GE(1, 1);
  DCHECK_GE(2, 1);
  DCHECK_LE(1, 1);
  DCHECK_LE(1, 2);
  DCHECK_GT(2, 1);
  DCHECK_LT(1, 2);

  // Test DCHECK on std::nullptr_t
  const void* p_null = nullptr;
  const void* p_not_null = &p_null;
  DCHECK_EQ(p_null, nullptr);
  DCHECK_EQ(nullptr, p_null);
  DCHECK_NE(p_not_null, nullptr);
  DCHECK_NE(nullptr, p_not_null);
}

TEST(CHECKTest, TestQCHECK) {
  // The tests that QCHECK does the same as CHECK
  QCHECK(1 == 1);  // NOLINT(readability/check)
  QCHECK_EQ(1, 1);
  QCHECK_NE(1, 2);
  QCHECK_GE(1, 1);
  QCHECK_GE(2, 1);
  QCHECK_LE(1, 1);
  QCHECK_LE(1, 2);
  QCHECK_GT(2, 1);
  QCHECK_LT(1, 2);

  // Tests using QCHECK*() on anonymous enums.
  QCHECK_EQ(CASE_A, CASE_A);
  QCHECK_NE(CASE_A, CASE_B);
  QCHECK_GE(CASE_A, CASE_A);
  QCHECK_GE(CASE_B, CASE_A);
  QCHECK_LE(CASE_A, CASE_A);
  QCHECK_LE(CASE_A, CASE_B);
  QCHECK_GT(CASE_B, CASE_A);
  QCHECK_LT(CASE_A, CASE_B);
}

TEST(CHECKTest, TestQCHECKPlacementsInCompoundStatements) {
  // check placement inside if/else clauses
  if (true) QCHECK(true);

  if (false)
    ;  // NOLINT
  else
    QCHECK(true);

  if (false)
    ;  // NOLINT
  else
    QCHECK(true);

  switch (0)
  case 0:
    QCHECK(true);

#if ABSL_INTERNAL_CPLUSPLUS_LANG >= 201703L
  constexpr auto var = [](int i) {
    QCHECK(i > 0);  // NOLINT
    return i + 1;
  }(global_var);
  (void)var;

#if defined(__GNUC__)
  int var2 = (({ CHECK_LE(1, 2); }), global_var < 10) ? 1 : 0;
  (void)var2;
#endif  // __GNUC__
#endif  // ABSL_INTERNAL_CPLUSPLUS_LANG
}

class ComparableType {
 public:
  explicit ComparableType(int v) : v_(v) {}

  void MethodWithCheck(int i) {
    CHECK_EQ(*this, i);
    CHECK_EQ(i, *this);
  }

  int Get() const { return v_; }

 private:
  friend bool operator==(const ComparableType& lhs, const ComparableType& rhs) {
    return lhs.v_ == rhs.v_;
  }
  friend bool operator!=(const ComparableType& lhs, const ComparableType& rhs) {
    return lhs.v_ != rhs.v_;
  }
  friend bool operator<(const ComparableType& lhs, const ComparableType& rhs) {
    return lhs.v_ < rhs.v_;
  }
  friend bool operator<=(const ComparableType& lhs, const ComparableType& rhs) {
    return lhs.v_ <= rhs.v_;
  }
  friend bool operator>(const ComparableType& lhs, const ComparableType& rhs) {
    return lhs.v_ > rhs.v_;
  }
  friend bool operator>=(const ComparableType& lhs, const ComparableType& rhs) {
    return lhs.v_ >= rhs.v_;
  }
  friend bool operator==(const ComparableType& lhs, int rhs) {
    return lhs.v_ == rhs;
  }
  friend bool operator==(int lhs, const ComparableType& rhs) {
    return lhs == rhs.v_;
  }

  friend std::ostream& operator<<(std::ostream& out, const ComparableType& v) {
    return out << "ComparableType{" << v.Get() << "}";
  }

  int v_;
};

TEST(CHECKTest, TestUserDefinedCompOp) {
  CHECK_EQ(ComparableType{0}, ComparableType{0});
  CHECK_NE(ComparableType{1}, ComparableType{2});
  CHECK_LT(ComparableType{1}, ComparableType{2});
  CHECK_LE(ComparableType{1}, ComparableType{2});
  CHECK_GT(ComparableType{2}, ComparableType{1});
  CHECK_GE(ComparableType{2}, ComparableType{2});
}

TEST(CHECKTest, TestCheckInMethod) {
  ComparableType v{1};
  v.MethodWithCheck(1);
}

TEST(CHECKDeathTest, TestUserDefinedStreaming) {
  ComparableType v1{1};
  ComparableType v2{2};

  EXPECT_DEATH(
      CHECK_EQ(v1, v2),
      HasSubstr(
          "Check failed: v1 == v2 (ComparableType{1} vs. ComparableType{2})"));
}

}  // namespace
