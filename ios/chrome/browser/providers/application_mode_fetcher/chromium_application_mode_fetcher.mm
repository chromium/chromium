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
                          NSString* app_id,
                          AppModeFetchingResponse fetching_response) {
  // Application update is not supported in Chromium.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(fetching_response), false, nil));
}
}  // namespace ios::provider
