// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/switcher_info/test_switcher_info.h"

#import <optional>

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/public/provider/chrome/browser/switcher_info/switcher_info_api.h"

namespace ios::provider {
namespace {
std::optional<SwitcherInfoResult> g_switcher_info_result = std::nullopt;
}  // namespace

void GetSwitcherInfo(id<SystemIdentity> identity,
                     SwitcherInfoCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), g_switcher_info_result));
}

namespace test {

void SetSwitcherInfoResult(std::optional<SwitcherInfoResult> result) {
  g_switcher_info_result = result;
}

void ResetSwitcherInfoState() {
  g_switcher_info_result = std::nullopt;
}

}  // namespace test
}  // namespace ios::provider
