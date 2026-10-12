// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/switcher_info/test_switcher_info.h"

#import <optional>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/public/provider/chrome/browser/switcher_info/switcher_info_api.h"

namespace ios::provider {
namespace {

using SwitcherInfoIdentityResults = std::vector<
    std::pair<id<SystemIdentity>, std::optional<SwitcherInfoResult>>>;

std::optional<SwitcherInfoResult> g_switcher_info_result = std::nullopt;

SwitcherInfoIdentityResults& GetSwitcherInfoIdentityResults() {
  static base::NoDestructor<SwitcherInfoIdentityResults> results;
  return *results;
}

}  // namespace

void GetSwitcherInfo(id<SystemIdentity> identity,
                     SwitcherInfoCallback callback) {
  std::optional<SwitcherInfoResult> result = g_switcher_info_result;
  for (const auto& entry : GetSwitcherInfoIdentityResults()) {
    if ([entry.first isEqual:identity]) {
      result = entry.second;
      break;
    }
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

namespace test {

void SetSwitcherInfoResult(std::optional<SwitcherInfoResult> result) {
  g_switcher_info_result = result;
}

void SetSwitcherInfoResultForIdentity(
    id<SystemIdentity> identity,
    std::optional<SwitcherInfoResult> result) {
  for (auto& entry : GetSwitcherInfoIdentityResults()) {
    if ([entry.first isEqual:identity]) {
      entry.second = result;
      return;
    }
  }
  GetSwitcherInfoIdentityResults().emplace_back(identity, result);
}

void ResetSwitcherInfoState() {
  g_switcher_info_result = std::nullopt;
  GetSwitcherInfoIdentityResults().clear();
}

}  // namespace test
}  // namespace ios::provider
