// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tips/ui/ntp_customization_promo_view_controller.h"

#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"

@implementation NTPCustomizationPromoViewController

- (instancetype)init {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  // TODO(crbug.com/571447764): Configure button titles and content views.
  return [super initWithConfiguration:configuration];
}

@end
