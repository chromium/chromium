// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_service_platform_delegate.h"

SupervisedUserServicePlatformDelegate::SupervisedUserServicePlatformDelegate() {
}

// TODO(crbug.com/1491611): migrate current implementation from
// IncognitoGridMediator::onPreferenceChanged().
void SupervisedUserServicePlatformDelegate::CloseIncognitoTabs() {}
