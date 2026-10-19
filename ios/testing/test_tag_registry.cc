// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/testing/test_tag_registry.h"

#include <set>
#include <string>
#include <string_view>

#include "base/no_destructor.h"

namespace testing {

// static
TestTagRegistry& TestTagRegistry::GetInstance() {
  static base::NoDestructor<TestTagRegistry> instance;
  return *instance;
}

TestTagRegistry::TestTagRegistry() = default;

TestTagRegistry::~TestTagRegistry() = default;

void TestTagRegistry::TagTest(std::string_view class_name,
                              std::string_view test_name,
                              std::string_view tag) {
  tagged_tests_.insert(
      {std::string(class_name), std::string(test_name), std::string(tag)});
}

bool TestTagRegistry::HasTag(std::string_view class_name,
                             std::string_view test_name,
                             std::string_view tag) const {
  return tagged_tests_.contains(TestTagEntry{
      std::string(class_name), std::string(test_name), std::string(tag)});
}

void TestTagRegistry::ClearForTesting() {
  tagged_tests_.clear();
}

TestTagRegistration::TestTagRegistration(std::string_view class_name,
                                         std::string_view test_name,
                                         std::string_view tag) {
  TestTagRegistry::GetInstance().TagTest(class_name, test_name, tag);
}

}  // namespace testing
