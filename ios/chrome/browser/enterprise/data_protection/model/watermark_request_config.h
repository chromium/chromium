// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_WATERMARK_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_WATERMARK_REQUEST_CONFIG_H_

#import <string>

#import "ios/chrome/browser/overlays/model/public/overlay_request_config.h"

// Configuration object for OverlayRequests displaying watermarks.
class WatermarkRequestConfig
    : public OverlayRequestConfig<WatermarkRequestConfig> {
 public:
  ~WatermarkRequestConfig() override;

  // The watermark text to be displayed.
  const std::string& watermark_text() const { return watermark_text_; }

 private:
  friend class OverlayUserData<WatermarkRequestConfig>;
  WatermarkRequestConfig(const std::string& watermark_text);

  std::string watermark_text_;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_WATERMARK_REQUEST_CONFIG_H_
