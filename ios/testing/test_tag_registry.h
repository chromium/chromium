// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_TEST_TAG_REGISTRY_H_
#define IOS_TESTING_TEST_TAG_REGISTRY_H_

#include <compare>
#include <set>
#include <string>
#include <string_view>

namespace testing {

// Common tag constant for smoke tests.
inline constexpr char kSmokeTag[] = "smoke";

// Registry that stores tags associated with tests.
// Used to categorize tests (e.g. "smoke") without encoding tag metadata
// into test method names.
class TestTagRegistry {
 public:
  // Returns the singleton instance of `TestTagRegistry`.
  static TestTagRegistry& GetInstance();

  TestTagRegistry();
  ~TestTagRegistry();

  TestTagRegistry(const TestTagRegistry&) = delete;
  TestTagRegistry& operator=(const TestTagRegistry&) = delete;

  // Registers `tag` for the test identified by `class_name` and `test_name`.
  void TagTest(std::string_view class_name,
               std::string_view test_name,
               std::string_view tag);

  // Returns true if the test identified by `class_name` and `test_name` is
  // tagged with `tag`.
  bool HasTag(std::string_view class_name,
              std::string_view test_name,
              std::string_view tag) const;

  // Clears all registered tags. Useful for resetting state between unit tests.
  void ClearForTesting();

 private:
  struct TestTagEntry {
    std::string class_name;
    std::string method_name;
    std::string tag;

    auto operator<=>(const TestTagEntry&) const = default;
  };

  std::set<TestTagEntry> tagged_tests_;
};

// Helper class to register a test tag at static initialization time.
class TestTagRegistration {
 public:
  TestTagRegistration(std::string_view class_name,
                      std::string_view test_name,
                      std::string_view tag);
  ~TestTagRegistration() = default;

  TestTagRegistration(const TestTagRegistration&) = delete;
  TestTagRegistration& operator=(const TestTagRegistration&) = delete;
};

}  // namespace testing

#if defined(__OBJC__)
// In Objective-C++, validates at compile time that `method_name` is a valid
// instance method on `class_name` (or its superclasses).
#define TAG_TEST_VALIDATE_METHOD(class_name, method_name, tag) \
  [[maybe_unused]] static inline void                          \
  tag_validate_##class_name##_##method_name##_##tag() {        \
    [static_cast<class_name*>(nullptr) method_name];           \
  }
#else
#define TAG_TEST_VALIDATE_METHOD(class_name, method_name, tag)
#endif

#define TAG_TEST_IMPL(class_name, method_name, tag)                           \
  TAG_TEST_VALIDATE_METHOD(class_name, method_name, tag)                      \
  [[maybe_unused]] static const ::testing::TestTagRegistration                \
      tag_reg_##class_name##_##method_name##_##tag(#class_name, #method_name, \
                                                   #tag)

// Macro to register an arbitrary tag for a test method at startup.
// Must be placed inside `@implementation` (or after `@end`), since EarlGrey
// test methods are typically not declared in `@interface`.
//
// Usage:
//   TAG_TEST(AccountMenuTestCase, testSignOut, smoke);
#define TAG_TEST(class_name, method_name, tag) \
  TAG_TEST_IMPL(class_name, method_name, tag)

// Macro to register a "smoke" tag for a test method at startup.
// Must be placed inside `@implementation` (or after `@end`), since EarlGrey
// test methods are typically not declared in `@interface`.
//
// Usage:
//   TAG_SMOKE_TEST(AccountMenuTestCase, testSignOut);
#define TAG_SMOKE_TEST(class_name, method_name) \
  TAG_TEST(class_name, method_name, smoke)

#endif  // IOS_TESTING_TEST_TAG_REGISTRY_H_
