// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/public/provider/chrome/browser/application_mode_fetcher/application_mode_fetcher_api.h"
#import "url/gurl.h"

namespace ios::provider {

void FetchApplicationMode(const GURL& url,
                          NSString* appID,
                          AppModeFetchingCallback callback) {
  // TODO(crbug.com/374934680): Add a factory to test different callback
  // configurations.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false));
}

}  // namespace ios::provider
