// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/target_process.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

void ExpectEnvironmentBlock(const std::vector<std::wstring>& vars,
                            const std::wstring& block) {
  std::wstring expected;
  for (const auto& var : vars) {
    expected += var;
    expected.push_back('\0');
  }
  expected.push_back('\0');
  EXPECT_EQ(expected, block);
}

}  // namespace

using TargetProcessTest = testing::Test;

TEST_F(TargetProcessTest, FilterEnvironment) {
  const wchar_t empty[] = {'\0'};
  const wchar_t a2b3c4[] = {'A',  '=', '2', '\0', 'B',  '=', '3',
                            '\0', 'C', '=', '4',  '\0', '\0'};

  // Empty filter should filter everything.
  std::vector<std::wstring_view> to_keep;
  auto res = TargetProcess::FilterEnvironment(empty, to_keep);
  ExpectEnvironmentBlock({}, res);

  res = TargetProcess::FilterEnvironment(a2b3c4, to_keep);
  ExpectEnvironmentBlock({}, res);

  to_keep.push_back(L"B");
  res = TargetProcess::FilterEnvironment(a2b3c4, to_keep);
  ExpectEnvironmentBlock({L"B=3"}, res);

  res = TargetProcess::FilterEnvironment(empty, to_keep);
  ExpectEnvironmentBlock({}, res);

  to_keep.push_back(L"D");
  // D should be ignored, but B should still appear.
  res = TargetProcess::FilterEnvironment(a2b3c4, to_keep);
  ExpectEnvironmentBlock({L"B=3"}, res);

  to_keep.clear();
  to_keep.push_back(L"D");
  // D should be ignored.
  res = TargetProcess::FilterEnvironment(a2b3c4, to_keep);
  ExpectEnvironmentBlock({}, res);

  to_keep.push_back(L"A");
  to_keep.push_back(L"C");
  // Once again D should be ignored but this time A and C should match.
  res = TargetProcess::FilterEnvironment(a2b3c4, to_keep);
  ExpectEnvironmentBlock({L"A=2", L"C=4"}, res);
}

}  // namespace sandbox
