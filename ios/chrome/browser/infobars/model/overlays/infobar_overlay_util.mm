// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue_util.h"

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
