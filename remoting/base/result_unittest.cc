// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/memory/raw_ptr_exclusion.h"
#include "remoting/base/result.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(Result, DefaultConstruction) {
  struct DefaultConstruct {
    std::string value;
    DefaultConstruct() : value("value1") {}
  };

  Result<DefaultConstruct, int> result;
  ASSERT_TRUE(result.is_success());
  EXPECT_EQ("value1", result.success().value);
}

namespace {
struct NotDefaultConstructible {
  NotDefaultConstructible(int);
};
}  // namespace

static_assert(
    !std::is_default_constructible<Result<NotDefaultConstructible, int>>::value,
    "Should not be default constructible if success type isn't.");

TEST(Result, TaggedSuccessConstruction) {
  Result<std::string, int> result(kSuccessTag, 5, 'a');
  ASSERT_TRUE(result.is_success());
  ASSERT_FALSE(result.is_error());
  EXPECT_EQ("aaaaa", result.success());
}

TEST(Result, TaggedErrorConstruction) {
  Result<std::string, int> result(kErrorTag, 2);
  ASSERT_FALSE(result.is_success());
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(2, result.error());
}

static_assert(
    !std::is_constructible<Result<std::string, int>, SuccessTag, float>::value,
    "Invalid constructor parameters should trigger SFINAE.");

TEST(Result, ImplicitSuccessConstruction) {
  Result<std::string, int> result = "value3";
  ASSERT_TRUE(result.is_success());
  EXPECT_EQ("value3", result.success());
}

TEST(Result, ImplicitErrorConstruction) {
  Result<std::string, int> result = 3;
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(3, result.error());
}

static_assert(!std::is_constructible<Result<int, float>, int>::value,
              "Should not allow ambiguous untagged construction.");

TEST(Result, SuccessCopyConstruction) {
  Result<std::string, int> result1 = "value4";
  Result<std::string, int> result2 = result1;
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ("value4", result2.success());
  // Ensure result1 wasn't modified.
  EXPECT_EQ("value4", result1.success());
}

TEST(Result, ErrorCopyConstruction) {
  Result<int, std::string> result1 = "value5";
  Result<int, std::string> result2 = result1;
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ("value5", result2.error());
  // Ensure result1 wasn't modified.
  EXPECT_EQ("value5", result1.error());
}

static_assert(
    !std::is_copy_constructible<Result<std::unique_ptr<int>, int>>::value &&
        !std::is_copy_constructible<Result<int, std::unique_ptr<int>>>::value,
    "Should not be copy constructible if either type isn't.");

TEST(Result, SuccessMoveConstruction) {
  Result<std::unique_ptr<std::string>, int> result1 =
      std::make_unique<std::string>("value6");
  Result<std::unique_ptr<std::string>, int> result2 = std::move(result1);
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ("value6", *result2.success());
  EXPECT_TRUE(result1.is_success());
  EXPECT_FALSE(result1.success());
}

TEST(Result, ErrorMoveConstruction) {
  Result<int, std::unique_ptr<std::string>> result1 =
      std::make_unique<std::string>("value7");
  Result<int, std::unique_ptr<std::string>> result2 = std::move(result1);
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ("value7", *result2.error());
  EXPECT_TRUE(result1.is_error());
  EXPECT_FALSE(result1.error());
}

TEST(Result, SuccessCopyConversion) {
  const char* value = "value8";
  Result<const char*, int> result1 = value;
  Result<std::string, int> result2 = result1;
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ("value8", result2.success());
  EXPECT_EQ(value, result1.success());
}

TEST(Result, ErrorCopyConversion) {
  const char* value = "value9";
  Result<int, const char*> result1 = value;
  Result<int, std::string> result2 = result1;
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ("value9", result2.error());
  EXPECT_EQ(value, result1.error());
}

TEST(Result, SuccessMoveConversion) {
  struct Deleter {
    Deleter(const std::default_delete<std::string>&) {}
    void operator()(void* ptr) { delete static_cast<std::string*>(ptr); }
  };

  Result<std::unique_ptr<std::string>, int> result1 =
      std::make_unique<std::string>("value10");
  Result<std::unique_ptr<void, Deleter>, int> result2 = std::move(result1);
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ("value10", *static_cast<std::string*>(result2.success().get()));
}

TEST(Result, ErrorMoveConversion) {
  struct Deleter {
    Deleter(const std::default_delete<std::string>&) {}
    void operator()(void* ptr) { delete static_cast<std::string*>(ptr); }
  };

  Result<int, std::unique_ptr<std::string>> result1 =
      std::make_unique<std::string>("value11");
  Result<int, std::unique_ptr<void, Deleter>> result2 = std::move(result1);
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ("value11", *static_cast<std::string*>(result2.error().get()));
}

TEST(Result, Destruction) {
  class DestructIncrement {
   public:
    explicit DestructIncrement(int* variable) : variable_(variable) {}
    ~DestructIncrement() { ++(*variable_); }

   private:
    // This field is not a raw_ptr<> because it was filtered by the rewriter
    // for: #union
    RAW_PTR_EXCLUSION int* variable_;
  };

  int success_count = 0;
  int error_count = 0;

  Result<DestructIncrement, int>(kSuccessTag, &success_count);
  EXPECT_EQ(1, success_count);
  EXPECT_EQ(0, error_count);

  Result<int, DestructIncrement>(kErrorTag, &error_count);
  EXPECT_EQ(1, success_count);
  EXPECT_EQ(1, error_count);
}

TEST(Result, CopyAssignment) {
  Result<std::string, int> result1 = "value12";
  Result<std::string, int> result2 = 0;
  result2 = result1;
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ("value12", result2.success());

  static_assert(
      !std::is_copy_assignable<Result<std::unique_ptr<int>, int>>::value &&
          !std::is_copy_assignable<Result<int, std::unique_ptr<int>>>::value,
      "Should not be copy assignable if either type isn't.");
}

TEST(Result, MoveAssignment) {
  Result<std::unique_ptr<std::string>, int> result1 =
      std::make_unique<std::string>("value13");
  Result<std::unique_ptr<std::string>, int> result2 = 0;
  result2 = std::move(result1);
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ("value13", *result2.success());
}

TEST(Result, CopyConversionAssignment) {
  const char* value1 = "value14";
  Result<const char*, int> result1 = value1;
  Result<std::string, int> result2 = 0;
  result2 = result1;
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ("value14", result2.success());
}

TEST(Result, MoveConversionAssignment) {
  struct Deleter {
    Deleter(const std::default_delete<std::string>&) {}
    void operator()(void* ptr) { delete static_cast<std::string*>(ptr); }
  };

  Result<std::unique_ptr<std::string>, int> result1 =
      std::make_unique<std::string>("value15");
  Result<std::unique_ptr<void, Deleter>, int> result2 = 0;
  result2 = std::move(result1);
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ("value15", *static_cast<std::string*>(result2.success().get()));
}

TEST(Result, EmplaceSuccess) {
  Result<std::string, int> result = 0;
  result.EmplaceSuccess(5, 'p');
  ASSERT_TRUE(result.is_success());
  EXPECT_EQ("ppppp", result.success());
}

TEST(Result, EmplaceError) {
  Result<std::string, int> result;
  result.EmplaceError(17);
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(17, result.error());
}

TEST(Result, MapLvalue) {
  Result<std::string, int> result1 = "value18";
  Result<const char*, int> result2 =
      result1.Map([](const std::string& value) { return value.c_str(); });
  ASSERT_TRUE(result2.is_success());
  EXPECT_TRUE(strcmp("value18", result2.success()) == 0);
}

TEST(Result, MapRvalue) {
  Result<std::unique_ptr<std::string>, int> result1 =
      std::make_unique<std::string>("value19");
  auto result2 = std::move(result1).Map(
      [](std::unique_ptr<std::string>&& value) { return std::move(*value); });
  static_assert(
      std::is_same<decltype(result2), Result<std::string, int>>::value,
      "Incorrect type inferred.");
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ("value19", result2.success());
}

TEST(Result, MapPassesErrorThrough) {
  Result<std::string, int> result1 = 20;
  Result<const char*, int> result2 =
      result1.Map([](const std::string& value) { return value.c_str(); });
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ(20, result2.error());
}

TEST(Result, MapErrorLvalue) {
  Result<std::string, int> result1 = 21;
  Result<std::string, int> result2 =
      result1.MapError([](int value) { return value + 1; });
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ(22, result2.error());
}

TEST(Result, MapErrorRvalue) {
  Result<int, std::unique_ptr<std::string>> result1 =
      std::make_unique<std::string>("value23");
  auto result2 =
      std::move(result1).MapError([](std::unique_ptr<std::string>&& value) {
        return std::make_unique<std::size_t>(value->size());
      });
  static_assert(std::is_same<decltype(result2),
                             Result<int, std::unique_ptr<std::size_t>>>::value,
                "Incorrect type inferred.");
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ(7UL, *result2.error());
}

TEST(Result, MapErrorPassesSuccessThrough) {
  Result<std::string, int> result1 = "value24";
  Result<std::string, float> result2 =
      result1.MapError([](int value) { return value * 2.0; });
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ("value24", result2.success());
}

TEST(Result, AndThenLvalue) {
  Result<std::string, int> result1 = "value25";
  auto result2 = result1.AndThen(
      [](const std::string&) { return Result<const char*, float>(26.0); });
  static_assert(
      std::is_same<decltype(result2), Result<const char*, int>>::value,
      "Error type should stay the same.");
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ(26, result2.error());
}

TEST(Result, AndThenRvalue) {
  Result<std::unique_ptr<std::string>, int> result1 =
      std::make_unique<std::string>("value27");
  auto result2 =
      std::move(result1).AndThen([](std::unique_ptr<std::string>&& value) {
        return Result<std::string, int>(std::move(*value));
      });
  static_assert(
      std::is_same<decltype(result2), Result<std::string, int>>::value,
      "Incorrect type inferred.");
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ("value27", result2.success());
}

TEST(Result, AndThenPassesErrorThrough) {
  Result<std::string, int> result1 = 28;
  Result<int, int> result2 = result1.AndThen(
      [](const std::string&) { return Result<int, int>(kSuccessTag, 29); });
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ(28, result2.error());
}

TEST(Result, OrElseLvalue) {
  Result<std::string, int> result1 = 30;
  Result<std::string, int> result2 =
      result1.OrElse([](int) -> Result<std::string, int> { return "value31"; });
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ("value31", result2.success());
}

TEST(Result, OrElseRvalue) {
  Result<int, std::unique_ptr<std::string>> result1 =
      std::make_unique<std::string>("value32");
  Result<int, std::string> result2 =
      std::move(result1).OrElse([](std::unique_ptr<std::string>&&) {
        return Result<int, std::string>("value33");
      });
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ("value33", result2.error());
}

TEST(Result, OrElsePassesSuccessThrough) {
  Result<std::string, int> result1 = "value34";
  Result<std::string, float> result2 = result1.OrElse(
      [](int value) -> Result<std::string, float> { return value * 2.0; });
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ("value34", result2.success());
}

TEST(Result, Visit) {
  struct Visitor {
    char success(const std::string& value) {
      EXPECT_EQ("value35", value);
      return '\1';
    }
    bool success(std::string& value) {
      EXPECT_EQ("value35", value);
      return true;
    }
    int success(std::string&& value) {
      EXPECT_EQ("value35", value);
      return 1;
    }
    char error(const std::wstring& value) {
      EXPECT_EQ(L"value36", value);
      return '\0';
    }
    bool error(std::wstring& value) {
      EXPECT_EQ(L"value36", value);
      return false;
    }
    int error(std::wstring&& value) {
      EXPECT_EQ(L"value36", value);
      return 0;
    }
  };

  Result<std::string, std::wstring> result;

  result.EmplaceSuccess("value35");
  auto success1 =
      const_cast<const Result<std::string, std::wstring>&>(result).Visit(
          Visitor());
  static_assert(std::is_same<char, decltype(success1)>::value,
                "Return type should be char.");
  EXPECT_EQ('\1', success1);
  auto success2 = result.Visit(Visitor());
  static_assert(std::is_same<bool, decltype(success2)>::value,
                "Return type should be bool.");
  EXPECT_EQ(true, success2);
  auto success3 = std::move(result).Visit(Visitor());
  static_assert(std::is_same<int, decltype(success3)>::value,
                "Return type should be int.");
  EXPECT_EQ(1, success3);

  result.EmplaceError(L"value36");
  auto error1 =
      const_cast<const Result<std::string, std::wstring>&>(result).Visit(
          Visitor());
  EXPECT_EQ('\0', error1);
  auto error2 = result.Visit(Visitor());
  EXPECT_EQ(false, error2);
  auto error3 = std::move(result).Visit(Visitor());
  EXPECT_EQ(0, error3);
}

}  // namespace remoting
