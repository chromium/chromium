// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/coordinator/watermark_overlay_mediator.h"

#import <algorithm>

#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/prefs/pref_service.h"
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
  WatermarkStyle style;
  if (_prefService) {
    // Sanitize Fill Opacity, expected to be [0, 100].
    int fillOpacityPercent = _prefService->GetInteger(
        enterprise_connectors::kWatermarkStyleFillOpacityPref);
    if (fillOpacityPercent < 0 || fillOpacityPercent > 100) {
      DLOG(WARNING) << "Invalid watermark fill opacity: " << fillOpacityPercent
                    << "%. Clamping to safe range [0, 100].";
      fillOpacityPercent = std::clamp(fillOpacityPercent, 0, 100);
    }
    style.fill_opacity = fillOpacityPercent / 100.0;

    // Sanitize Outline Opacity, expected to be [0, 100]%.
    int outlineOpacityPercent = _prefService->GetInteger(
        enterprise_connectors::kWatermarkStyleOutlineOpacityPref);
    if (outlineOpacityPercent < 0 || outlineOpacityPercent > 100) {
      DLOG(WARNING) << "Invalid watermark outline opacity: "
                    << outlineOpacityPercent
                    << "%. Clamping to safe range [0, 100].";
      outlineOpacityPercent = std::clamp(outlineOpacityPercent, 0, 100);
    }
    style.outline_opacity = outlineOpacityPercent / 100.0;

    // Sanitize Font Size, expected to be [1, 500]pt.
    int fontSize = _prefService->GetInteger(
        enterprise_connectors::kWatermarkStyleFontSizePref);
    if (fontSize < 1 || fontSize > 500) {
      DLOG(WARNING) << "Invalid watermark font size: " << fontSize
                    << "pt. Clamping to safe range [1, 500].";
      fontSize = std::clamp(fontSize, 1, 500);
    }
    style.font_size = fontSize;
  }

  [self.consumer updateWatermarkWithText:watermarkText style:style];
}

@end
