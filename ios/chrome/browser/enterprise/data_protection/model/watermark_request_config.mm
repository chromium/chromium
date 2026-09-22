// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/model/watermark_request_config.h"

WatermarkRequestConfig::WatermarkRequestConfig(
    const std::string& watermark_text)
    : watermark_text_(watermark_text) {}

WatermarkRequestConfig::~WatermarkRequestConfig() = default;
