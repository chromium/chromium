// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/result.h"

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(Result, DefaultConstruction) {
  struct DefaultConstruct {
    std::string value;
    DefaultConstruct() : value("value1") {}
  };

  Result<DefaultConstruct, int> result;
  ASSERT_TRUE(result.is_success());
  EXPECT_EQ(result.success().value, "value1");
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
  EXPECT_EQ(result.success(), "aaaaa");
}

TEST(Result, TaggedErrorConstruction) {
  Result<std::string, int> result(kErrorTag, 2);
  ASSERT_FALSE(result.is_success());
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(result.error(), 2);
}

static_assert(
    !std::is_constructible<Result<std::string, int>, SuccessTag, float>::value,
    "Invalid constructor parameters should trigger SFINAE.");

TEST(Result, ImplicitSuccessConstruction) {
  Result<std::string, int> result = "value3";
  ASSERT_TRUE(result.is_success());
  EXPECT_EQ(result.success(), "value3");
}

TEST(Result, ImplicitErrorConstruction) {
  Result<std::string, int> result = 3;
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(result.error(), 3);
}

static_assert(!std::is_constructible<Result<int, float>, int>::value,
              "Should not allow ambiguous untagged construction.");

TEST(Result, SuccessCopyConstruction) {
  Result<std::string, int> result1 = "value4";
  Result<std::string, int> result2 = result1;
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ(result2.success(), "value4");
  // Ensure result1 wasn't modified.
  EXPECT_EQ(result1.success(), "value4");
}

TEST(Result, ErrorCopyConstruction) {
  Result<int, std::string> result1 = "value5";
  Result<int, std::string> result2 = result1;
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ(result2.error(), "value5");
  // Ensure result1 wasn't modified.
  EXPECT_EQ(result1.error(), "value5");
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
  EXPECT_EQ(*result2.success(), "value6");
  EXPECT_TRUE(result1.is_success());
  EXPECT_FALSE(result1.success());
}

TEST(Result, ErrorMoveConstruction) {
  Result<int, std::unique_ptr<std::string>> result1 =
      std::make_unique<std::string>("value7");
  Result<int, std::unique_ptr<std::string>> result2 = std::move(result1);
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ(*result2.error(), "value7");
  EXPECT_TRUE(result1.is_error());
  EXPECT_FALSE(result1.error());
}

TEST(Result, SuccessCopyConversion) {
  const char* value = "value8";
  Result<const char*, int> result1 = value;
  Result<std::string, int> result2 = result1;
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ(result2.success(), "value8");
  EXPECT_EQ(result1.success(), value);
}

TEST(Result, ErrorCopyConversion) {
  const char* value = "value9";
  Result<int, const char*> result1 = value;
  Result<int, std::string> result2 = result1;
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ(result2.error(), "value9");
  EXPECT_EQ(result1.error(), value);
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
  EXPECT_EQ(*static_cast<std::string*>(result2.success().get()), "value10");
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
  EXPECT_EQ(*static_cast<std::string*>(result2.error().get()), "value11");
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
  EXPECT_EQ(success_count, 1);
  EXPECT_EQ(error_count, 0);

  Result<int, DestructIncrement>(kErrorTag, &error_count);
  EXPECT_EQ(success_count, 1);
  EXPECT_EQ(error_count, 1);
}

TEST(Result, CopyAssignment) {
  Result<std::string, int> result1 = "value12";
  Result<std::string, int> result2 = 0;
  result2 = result1;
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ(result2.success(), "value12");

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
  EXPECT_EQ(*result2.success(), "value13");
}

TEST(Result, CopyConversionAssignment) {
  const char* value1 = "value14";
  Result<const char*, int> result1 = value1;
  Result<std::string, int> result2 = 0;
  result2 = result1;
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ(result2.success(), "value14");
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
  EXPECT_EQ(*static_cast<std::string*>(result2.success().get()), "value15");
}

TEST(Result, EmplaceSuccess) {
  Result<std::string, int> result = 0;
  result.EmplaceSuccess(5, 'p');
  ASSERT_TRUE(result.is_success());
  EXPECT_EQ(result.success(), "ppppp");
}

TEST(Result, EmplaceError) {
  Result<std::string, int> result;
  result.EmplaceError(17);
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(result.error(), 17);
}

TEST(Result, MapLvalue) {
  Result<std::string, int> result1 = "value18";
  Result<const char*, int> result2 =
      result1.Map([](const std::string& value) { return value.c_str(); });
  ASSERT_TRUE(result2.is_success());
  UNSAFE_TODO(EXPECT_TRUE(strcmp("value18", result2.success()) == 0));
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
  EXPECT_EQ(result2.success(), "value19");
}

TEST(Result, MapPassesErrorThrough) {
  Result<std::string, int> result1 = 20;
  Result<const char*, int> result2 =
      result1.Map([](const std::string& value) { return value.c_str(); });
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ(result2.error(), 20);
}

TEST(Result, MapErrorLvalue) {
  Result<std::string, int> result1 = 21;
  Result<std::string, int> result2 =
      result1.MapError([](int value) { return value + 1; });
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ(result2.error(), 22);
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
  EXPECT_EQ(*result2.error(), 7UL);
}

TEST(Result, MapErrorPassesSuccessThrough) {
  Result<std::string, int> result1 = "value24";
  Result<std::string, float> result2 =
      result1.MapError([](int value) { return value * 2.0; });
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ(result2.success(), "value24");
}

TEST(Result, AndThenLvalue) {
  Result<std::string, int> result1 = "value25";
  auto result2 = result1.AndThen(
      [](const std::string&) { return Result<const char*, float>(26.0); });
  static_assert(
      std::is_same<decltype(result2), Result<const char*, int>>::value,
      "Error type should stay the same.");
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ(result2.error(), 26);
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
  EXPECT_EQ(result2.success(), "value27");
}

TEST(Result, AndThenPassesErrorThrough) {
  Result<std::string, int> result1 = 28;
  Result<int, int> result2 = result1.AndThen(
      [](const std::string&) { return Result<int, int>(kSuccessTag, 29); });
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ(result2.error(), 28);
}

TEST(Result, OrElseLvalue) {
  Result<std::string, int> result1 = 30;
  Result<std::string, int> result2 =
      result1.OrElse([](int) -> Result<std::string, int> { return "value31"; });
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ(result2.success(), "value31");
}

TEST(Result, OrElseRvalue) {
  Result<int, std::unique_ptr<std::string>> result1 =
      std::make_unique<std::string>("value32");
  Result<int, std::string> result2 =
      std::move(result1).OrElse([](std::unique_ptr<std::string>&&) {
        return Result<int, std::string>("value33");
      });
  ASSERT_TRUE(result2.is_error());
  EXPECT_EQ(result2.error(), "value33");
}

TEST(Result, OrElsePassesSuccessThrough) {
  Result<std::string, int> result1 = "value34";
  Result<std::string, float> result2 = result1.OrElse(
      [](int value) -> Result<std::string, float> { return value * 2.0; });
  ASSERT_TRUE(result2.is_success());
  EXPECT_EQ(result2.success(), "value34");
}

TEST(Result, Visit) {
  struct Visitor {
    char success(const std::string& value) {
      EXPECT_EQ(value, "value35");
      return '\1';
    }
    bool success(std::string& value) {
      EXPECT_EQ(value, "value35");
      return true;
    }
    int success(std::string&& value) {
      EXPECT_EQ(value, "value35");
      return 1;
    }
    char error(const std::wstring& value) {
      EXPECT_EQ(value, L"value36");
      return '\0';
    }
    bool error(std::wstring& value) {
      EXPECT_EQ(value, L"value36");
      return false;
    }
    int error(std::wstring&& value) {
      EXPECT_EQ(value, L"value36");
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
  EXPECT_EQ(success1, '\1');
  auto success2 = result.Visit(Visitor());
  static_assert(std::is_same<bool, decltype(success2)>::value,
                "Return type should be bool.");
  EXPECT_EQ(success2, true);
  auto success3 = std::move(result).Visit(Visitor());
  static_assert(std::is_same<int, decltype(success3)>::value,
                "Return type should be int.");
  EXPECT_EQ(success3, 1);

  result.EmplaceError(L"value36");
  auto error1 =
      const_cast<const Result<std::string, std::wstring>&>(result).Visit(
          Visitor());
  EXPECT_EQ(error1, '\0');
  auto error2 = result.Visit(Visitor());
  EXPECT_EQ(error2, false);
  auto error3 = std::move(result).Visit(Visitor());
  EXPECT_EQ(error3, 0);
}

}  // namespace remoting
