// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/fake_infobar_overlay_request_factory.h"

#import "base/check.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"

std::unique_ptr<OverlayRequest> FakeInfobarOverlayRequestFactory(
    InfoBarIOS* infobar_ios,
    InfobarOverlayType overlay_type) {
  DCHECK(infobar_ios);
  return OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
      infobar_ios, overlay_type, infobar_ios->high_priority());
}
