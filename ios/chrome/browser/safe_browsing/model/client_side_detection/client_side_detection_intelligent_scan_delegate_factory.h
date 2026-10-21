// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_FACTORY_H_

#import <memory>

#import "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns `IntelligentScanDelegate` objects, one for each active
// Profile.
class ClientSideDetectionIntelligentScanDelegateFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          ClientSideDetectionIntelligentScanDelegateFactory,
          safe_browsing::IntelligentScanDelegate> {
 public:
  explicit ClientSideDetectionIntelligentScanDelegateFactory(PassKey key);

 private:
  // `ProfileKeyedServiceFactoryIOS` implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_FACTORY_H_
