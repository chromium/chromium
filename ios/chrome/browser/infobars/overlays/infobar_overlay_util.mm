// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"

#import "base/bind.h"
#import "base/check.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

InfoBarIOS* GetOverlayRequestInfobar(OverlayRequest* request) {
  InfobarOverlayRequestConfig* config =
      request->GetConfig<InfobarOverlayRequestConfig>();
  return config ? config->infobar() : nullptr;
}

InfobarType GetOverlayRequestInfobarType(OverlayRequest* request) {
  return request->GetConfig<InfobarOverlayRequestConfig>()->infobar_type();
}

InfobarOverlayType GetOverlayRequestInfobarOverlayType(
    OverlayRequest* request) {
  return request->GetConfig<InfobarOverlayRequestConfig>()->overlay_type();
}

bool GetInfobarOverlayRequestIndex(OverlayRequestQueue* queue,
                                   InfoBarIOS* infobar,
                                   size_t* index) {
  return GetIndexOfMatchingRequest(
      queue, index,
      base::BindRepeating(
          [](InfoBarIOS* infobar, OverlayRequest* request) -> bool {
            return GetOverlayRequestInfobar(request) == infobar;
          },
          infobar));
}
