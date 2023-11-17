// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/mailto_handler/mailto_handler_api.h"

#import <UIKit/UIKit.h>

#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service.h"

namespace ios {
namespace provider {
namespace {

// Dummy MailtoHandlerService implementation used for Chromium builds.
class ChromiumMailtoHandlerService final : public MailtoHandlerService {
 public:
  // MailtoHandlerService implementation.
  NSString* SettingsTitle() const final;
  UIViewController* CreateSettingsController() final;
  void DismissAllMailtoHandlerInterfaces() final;
  void HandleMailtoURL(NSURL* url, base::OnceClosure completion) final;
};

NSString* ChromiumMailtoHandlerService::SettingsTitle() const {
  return nil;
}

UIViewController* ChromiumMailtoHandlerService::CreateSettingsController() {
  return nil;
}

void ChromiumMailtoHandlerService::DismissAllMailtoHandlerInterfaces() {
  // nothing to do
}

void ChromiumMailtoHandlerService::HandleMailtoURL(
    NSURL* url,
    base::OnceClosure completion) {
  auto callback = base::IgnoreArgs<BOOL>(std::move(completion));
  [[UIApplication sharedApplication]
                openURL:url
                options:@{}
      completionHandler:base::CallbackToBlock(std::move(callback))];
}

}  // namespace

std::unique_ptr<MailtoHandlerService> CreateMailtoHandlerService(
    MailtoHandlerConfiguration* configuration) {
  return std::make_unique<ChromiumMailtoHandlerService>();
}

}  // namespace provider
}  // namespace ios
