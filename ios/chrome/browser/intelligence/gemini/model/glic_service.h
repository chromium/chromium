// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_MODEL_GLIC_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_MODEL_GLIC_SERVICE_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"

class AuthenticationService;

// A browser-context keyed service for Glic.
class GlicService : public KeyedService {
 public:
  GlicService(AuthenticationService* auth_service);
  ~GlicService() override;

  // Presents the overlay on a given view controller.
  void PresentOverlayOnViewController(
      UIViewController* base_view_controller,
      std::unique_ptr<optimization_guide::proto::PageContext> page_context);

 private:
  // AuthenticationService used to check if a user is signed in or not.
  raw_ptr<AuthenticationService> auth_service_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_MODEL_GLIC_SERVICE_H_
