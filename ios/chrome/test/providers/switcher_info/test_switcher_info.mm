// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/public/provider/chrome/browser/switcher_info/switcher_info_api.h"

namespace ios::provider {

void GetSwitcherInfo(id<SystemIdentity> identity,
                     SwitcherInfoCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
}

}  // namespace ios::provider
