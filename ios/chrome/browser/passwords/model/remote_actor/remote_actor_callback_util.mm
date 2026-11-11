// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_callback_util.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <string_view>
#import <utility>

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/notreached.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_constants.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

namespace {

std::string_view StatusToQueryValue(RemoteActorCallbackStatus status) {
  switch (status) {
    case RemoteActorCallbackStatus::kSuccess:
      return "SUCCESS";
    case RemoteActorCallbackStatus::kCancelled:
      return "CANCELLED";
    case RemoteActorCallbackStatus::kFailed:
      return "FAILED";
  }
  NOTREACHED();
}

}  // namespace

void OpenGeminiCallbackURL(std::string_view task_id,
                           RemoteActorCallbackStatus status,
                           base::OnceCallback<void(bool)> completion) {
  CHECK([NSThread isMainThread]);
  CHECK(completion);

  if (task_id.empty()) {
    std::move(completion).Run(false);
    return;
  }

  GURL url = net::AppendQueryParameter(GURL(kRemoteActorGeminiCallbackBaseURL),
                                       kRemoteActorTaskIdQueryParam, task_id);
  url = net::AppendQueryParameter(url, kRemoteActorStatusQueryParam,
                                  StatusToQueryValue(status));

  NSURL* callback_url = net::NSURLWithGURL(url);
  if (!callback_url) {
    std::move(completion).Run(false);
    return;
  }

  [[UIApplication sharedApplication]
                openURL:callback_url
                options:@{}
      completionHandler:base::CallbackToBlock(std::move(completion))];
}
