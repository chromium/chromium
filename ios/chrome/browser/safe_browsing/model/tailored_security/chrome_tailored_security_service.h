// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}

namespace safe_browsing {

// TailoredSecurityService for iOS. This class is used to bridge
// communication between Account-level Enhanced Safe Browsing and Chrome-level
// Enhanced Safe Browsing. It also provides functionality to sync these two
// features.
class ChromeTailoredSecurityService : public TailoredSecurityService {
 public:
  explicit ChromeTailoredSecurityService(
      ProfileIOS* profile,
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service);
  ~ChromeTailoredSecurityService() override;

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

 private:
  // Called when the app has been backgrounded.
  void AppDidEnterBackground();

  // Called when the app has been foregrounded.
  void AppWillEnterForeground();

  raw_ptr<ProfileIOS> profile_;

  // Observers for NSNotificationCenter notifications.
  id application_backgrounding_observer_;
  id application_foregrounding_observer_;

  base::WeakPtrFactory<ChromeTailoredSecurityService> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_
