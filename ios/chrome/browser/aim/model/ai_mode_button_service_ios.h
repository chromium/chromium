// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_MODEL_AI_MODE_BUTTON_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_AIM_MODEL_AI_MODE_BUTTON_SERVICE_IOS_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "url/gurl.h"

class TemplateURLService;

// KeyedService providing properties of the AI Mode NTP button on iOS.
class AIModeButtonServiceIOS : public KeyedService {
 public:
  explicit AIModeButtonServiceIOS(TemplateURLService* template_url_service);
  AIModeButtonServiceIOS(const AIModeButtonServiceIOS&) = delete;
  AIModeButtonServiceIOS& operator=(const AIModeButtonServiceIOS&) = delete;
  ~AIModeButtonServiceIOS() override;

  // The title for the AI Mode button.
  NSString* GetTitle() const;

  // The icon for the AI Mode button.
  UIImage* GetIcon() const;

  // The URL to navigate to when the AI Mode button is tapped.
  GURL GetUrl() const;

 private:
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_AI_MODE_BUTTON_SERVICE_IOS_H_
