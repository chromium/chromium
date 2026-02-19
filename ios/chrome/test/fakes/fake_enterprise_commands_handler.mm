// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_enterprise_commands_handler.h"

@implementation FakeEnterpriseCommandsHandler
@synthesize dialogType = _dialogType;
@synthesize organizationDomain = _organizationDomain;

- (void)showEnterpriseWarningDialog:(enterprise::DialogType)dialogType
                 organizationDomain:(std::string_view)organizationDomain
                           callback:(base::OnceCallback<void(bool)>)callback {
  _dialogType = dialogType;
  _organizationDomain = std::string(organizationDomain);
  _callback = std::move(callback);
}

@end
