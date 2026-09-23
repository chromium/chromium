// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class KeyedService;
class ProfileIOS;

namespace safe_browsing {
class IntelligentScanDelegate;
}  // namespace safe_browsing

// Singleton that owns `IntelligentScanDelegate` objects, one for each active
// Profile.
class ClientSideDetectionIntelligentScanDelegateFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  // Creates the service if it doesn't exist already for the given `profile`.
  // If the service already exists, return its pointer.
  static safe_browsing::IntelligentScanDelegate* GetForProfile(
      ProfileIOS* profile);

  // Returns the singleton instance of this factory.
  static ClientSideDetectionIntelligentScanDelegateFactory* GetInstance();

  ClientSideDetectionIntelligentScanDelegateFactory(
      const ClientSideDetectionIntelligentScanDelegateFactory&) = delete;
  ClientSideDetectionIntelligentScanDelegateFactory& operator=(
      const ClientSideDetectionIntelligentScanDelegateFactory&) = delete;

 private:
  friend class base::NoDestructor<
      ClientSideDetectionIntelligentScanDelegateFactory>;

  ClientSideDetectionIntelligentScanDelegateFactory();
  ~ClientSideDetectionIntelligentScanDelegateFactory() override;

  // `ProfileKeyedServiceFactoryIOS` implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_FACTORY_H_
