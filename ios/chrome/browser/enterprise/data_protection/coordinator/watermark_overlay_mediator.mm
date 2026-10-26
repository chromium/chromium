// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/coordinator/watermark_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/enterprise/data_protection/coordinator/watermark_style_utils.h"
#import "ios/chrome/browser/enterprise/data_protection/model/watermark_request_config.h"
#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_consumer.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"

@implementation WatermarkOverlayMediator {
  // The overlay request to display watermark overlay.
  raw_ptr<OverlayRequest> _request;
  // The preference service to retrieve watermark style customisation options
  raw_ptr<PrefService> _prefService;
}

- (instancetype)initWithRequest:(OverlayRequest*)request
                    prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _request = request;
    _prefService = prefService;
  }
  return self;
}

- (void)setConsumer:(id<WatermarkConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  if (_consumer) {
    [self updateWatermark];
  }
}

- (void)disconnect {
  _consumer = nil;
}

#pragma mark - Private

// Fetches the latest watermark configuration, and updates the watermark based
// on the configuration.
- (void)updateWatermark {
  if (!_request) {
    return;
  }
  WatermarkRequestConfig* config =
      _request->GetConfig<WatermarkRequestConfig>();
  if (!config) {
    return;
  }

  NSString* watermarkText = base::SysUTF8ToNSString(config->watermark_text());
  WatermarkStyle style = GetWatermarkStyleFromPrefs(_prefService);
  [self.consumer updateWatermarkWithText:watermarkText style:style];
}

@end
