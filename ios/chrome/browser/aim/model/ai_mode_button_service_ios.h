// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_MODEL_AI_MODE_BUTTON_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_AIM_MODEL_AI_MODE_BUTTON_SERVICE_IOS_H_

#import <UIKit/UIKit.h>

#import "base/callback_list.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/search_engines/template_url_service_observer.h"
#import "url/gurl.h"

class AimEligibilityService;
class TemplateURLService;

// KeyedService providing properties of the AI Mode NTP button on iOS.
class AIModeButtonServiceIOS : public KeyedService,
                               public TemplateURLServiceObserver {
 public:
  AIModeButtonServiceIOS(TemplateURLService* template_url_service,
                         AimEligibilityService* aim_eligibility_service);
  AIModeButtonServiceIOS(const AIModeButtonServiceIOS&) = delete;
  AIModeButtonServiceIOS& operator=(const AIModeButtonServiceIOS&) = delete;
  ~AIModeButtonServiceIOS() override;

  // KeyedService:
  void Shutdown() override;

  // Whether the AI Mode button is available on the NTP.
  bool IsButtonAvailable() const;

  // The title for the AI Mode button.
  NSString* GetTitle() const;

  // The icon for the AI Mode button.
  UIImage* GetIcon() const;

  // The URL to navigate to when the AI Mode button is tapped.
  GURL GetUrl() const;

  // Registers a callback to be called when the button state (availability,
  // icon, title, etc.) changes.
  base::CallbackListSubscription RegisterStateChangedCallback(
      base::RepeatingClosure callback);

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

 private:
  // Called when the eligibility service notifies that AIM eligibility changed.
  void OnEligibilityChanged();

  // Notifies all registered state change observers.
  void NotifyStateChanged();

  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  raw_ptr<AimEligibilityService> aim_eligibility_service_ = nullptr;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};
  base::CallbackListSubscription eligibility_subscription_;

  base::RepeatingClosureList state_changed_callbacks_;
};

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_AI_MODE_BUTTON_SERVICE_IOS_H_
