// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/glic/model/glic_service.h"

#import <memory>

#import "ios/public/provider/chrome/browser/glic/glic_api.h"

GlicService::GlicService() = default;

GlicService::~GlicService() = default;

void GlicService::PresentOverlayOnViewController(
    UIViewController* base_view_controller) {
  ios::provider::StartOverlay(base_view_controller);
}
