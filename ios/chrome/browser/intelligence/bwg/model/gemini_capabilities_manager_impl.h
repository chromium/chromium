// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CAPABILITIES_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CAPABILITIES_MANAGER_IMPL_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"

class AuthenticationService;
class GeminiService;
class ProfileIOS;
@class NSMutableDictionary;
@class NSUserDefaults;

class GeminiCapabilitiesManagerImpl : public GeminiCapabilitiesManager,
                                      public GeminiService::Observer {
 public:
  GeminiCapabilitiesManagerImpl(ProfileIOS* profile,
                                AuthenticationService* authentication_service,
                                GeminiService* gemini_service);
  ~GeminiCapabilitiesManagerImpl() override;

  // KeyedService implementation.
  void Shutdown() override;

  // GeminiCapabilitiesManager implementation.
  void UpdateCapabilities() override;

  // GeminiService::Observer implementation.
  void OnGeminiEligibilityChanged() override;

 private:
  // Helper methods to update specific capabilities.
  void UpdateSupportsAISummarization(NSMutableDictionary* capabilities);
  void UpdateHashedUserID(NSUserDefaults* shared_defaults,
                          bool has_primary_identity);
  void UpdateUserEligibility(NSMutableDictionary* capabilities,
                             bool user_eligible,
                             bool has_primary_identity);
  // Profile associated with this manager.
  raw_ptr<ProfileIOS> profile_;

  // AuthenticationService used to retrieve primary identity.
  raw_ptr<AuthenticationService> authentication_service_;

  // GeminiService used to query user eligibility.
  raw_ptr<GeminiService> gemini_service_;

  // Scoped observation for GeminiService.
  base::ScopedObservation<GeminiService, GeminiService::Observer>
      gemini_service_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CAPABILITIES_MANAGER_IMPL_H_
