// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_SPINNING_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_SPINNING_OVERLAY_REQUEST_CONFIG_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/overlays/model/public/overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response_info.h"

// Configuration object for OverlayRequests for spinning overlays.
class SpinningOverlayRequestConfig
    : public OverlayRequestConfig<SpinningOverlayRequestConfig> {
 public:
  ~SpinningOverlayRequestConfig() override;

  // The text to be displayed on the overlay. Returns nil if no label should be
  // shown.
  NSString* label_text() const { return label_text_; }

  // Whether tapping the overlay cancels it.
  bool is_cancellable() const { return is_cancellable_; }

 private:
  friend class OverlayUserData<SpinningOverlayRequestConfig>;

  // Creates a configuration with `label_text` and whether it `is_cancellable`.
  // `label_text` is optional and can be nil if no label should be displayed.
  SpinningOverlayRequestConfig(NSString* label_text, bool is_cancellable);

  NSString* label_text_ = nil;
  bool is_cancellable_ = false;
};

// Response info object sent when the spinning overlay is dismissed.
class SpinningOverlayResponse
    : public OverlayResponseInfo<SpinningOverlayResponse> {
 public:
  ~SpinningOverlayResponse() override;

  // Whether the overlay was canceled by user interaction.
  bool canceled() const { return canceled_; }

 private:
  friend class OverlayUserData<SpinningOverlayResponse>;

  // Creates a response with whether the overlay was `canceled`.
  explicit SpinningOverlayResponse(bool canceled);

  bool canceled_ = false;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_SPINNING_OVERLAY_REQUEST_CONFIG_H_
