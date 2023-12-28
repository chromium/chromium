// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ssl/model/insecure_form_warning_app_interface.h"

#import "components/security_interstitials/core/insecure_form_util.h"

@implementation InsecureFormWarningAppInterface

+ (void)setInsecureFormPortsForTesting:(int)portTreatedAsSecure
                 portTreatedAsInsecure:(int)portTreatedAsInsecure {
  security_interstitials::SetInsecureFormPortsForTesting(portTreatedAsSecure,
                                                         portTreatedAsInsecure);
}

@end
