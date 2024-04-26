// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/model/translate_infobar_metrics_recorder.h"

#import "base/metrics/histogram_macros.h"

@implementation TranslateInfobarMetricsRecorder

+ (void)recordBannerEvent:(MobileMessagesTranslateBannerEvent)bannerEvent {
  // TODO(crbug.com/40107868): Use function version of macros.
  UMA_HISTOGRAM_ENUMERATION("Mobile.Messages.Translate.Banner.Event",
                            bannerEvent);
}

+ (void)recordModalEvent:(MobileMessagesTranslateModalEvent)modalEvent {
  // TODO(crbug.com/40107868): Use function version of macros.
  UMA_HISTOGRAM_ENUMERATION("Mobile.Messages.Translate.Modal.Event",
                            modalEvent);
}

+ (void)recordModalPresent:(MobileMessagesTranslateModalPresent)presentEvent {
  // TODO(crbug.com/40107868): Use function version of macros.
  UMA_HISTOGRAM_ENUMERATION("Mobile.Messages.Translate.Modal.Present",
                            presentEvent);
}

+ (void)recordUnusedInfobar {
  // TODO(crbug.com/40107868): Use function version of macros.
  UMA_HISTOGRAM_COUNTS_10M("Mobile.Translate.Unused.Count", 1);
}

@end
