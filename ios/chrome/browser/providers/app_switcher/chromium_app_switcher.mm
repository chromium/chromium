// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/public/provider/chrome/browser/app_switcher/app_switcher_api.h"

namespace ios::provider {

void FetchAppSwitcherParams(const GURL& url,
                            std::string_view app_id,
                            AppSwitcherResponseCallback callback) {
  // App switcher parameters fetching is not supported in Chromium.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), AppSwitcherUrlOpeningResult{}));
}

}  // namespace ios::provider
