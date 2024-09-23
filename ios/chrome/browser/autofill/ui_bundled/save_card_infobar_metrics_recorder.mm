// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/save_card_infobar_metrics_recorder.h"

#import "base/metrics/histogram_macros.h"

@implementation SaveCardInfobarMetricsRecorder

+ (void)recordModalEvent:(MobileMessagesSaveCardModalEvent)event {
  UMA_HISTOGRAM_ENUMERATION("Mobile.Messages.Save.Card.Modal.Event", event);
}

@end
