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

class AiModeButtonService;
struct AiModeButtonUiConfig;
class AimEligibilityService;
class TemplateURLService;

// KeyedService providing properties of the AI Mode NTP button on iOS.
class AIModeButtonServiceIOS : public KeyedService,
                               public TemplateURLServiceObserver {
 public:
  AIModeButtonServiceIOS(TemplateURLService* template_url_service,
                         AimEligibilityService* aim_eligibility_service,
                         AiModeButtonService* ai_mode_button_service);
  AIModeButtonServiceIOS(const AIModeButtonServiceIOS&) = delete;
  AIModeButtonServiceIOS& operator=(const AIModeButtonServiceIOS&) = delete;
  ~AIModeButtonServiceIOS() override;

  // KeyedService:
  void Shutdown() override;

  // Whether the AI Mode button is available on the NTP.
  bool IsButtonAvailable() const;

  // The title for the AI Mode button.
  NSString* GetTitle() const;

  // The accessibility label for the AI Mode button.
  NSString* GetAccessibilityLabel() const;

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
  // Returns the 3P AI mode button UI config if the default search provider is
  // not Google and a valid config is available, or nullptr otherwise.
  const AiModeButtonUiConfig* GetThirdPartyConfig() const;

  // Called when the eligibility service notifies that AIM eligibility changed.
  void OnEligibilityChanged();

  // Called when the AI mode button service notifies that config changed.
  void OnAiModeButtonConfigChanged(const AiModeButtonUiConfig* config);

  // Notifies all registered state change observers.
  void NotifyStateChanged();

  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  raw_ptr<AimEligibilityService> aim_eligibility_service_ = nullptr;
  raw_ptr<AiModeButtonService> ai_mode_button_service_ = nullptr;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};
  base::CallbackListSubscription eligibility_subscription_;
  base::CallbackListSubscription ai_mode_button_subscription_;

  base::RepeatingClosureList state_changed_callbacks_;
};

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_AI_MODE_BUTTON_SERVICE_IOS_H_
