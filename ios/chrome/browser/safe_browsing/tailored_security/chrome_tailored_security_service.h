// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_

#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"

class ChromeBrowserState;

namespace signin {
class IdentityManager;
}

namespace safe_browsing {

// TailoredSecurityService for iOS. This class is used to bridge
// communication between Account-level Enhanced Safe Browsing and Chrome-level
// Enhanced Safe Browsing. It also provides functionality to sync these two
// features.
class ChromeTailoredSecurityService : public TailoredSecurityService {
 public:
  explicit ChromeTailoredSecurityService(
      ChromeBrowserState* state,
      signin::IdentityManager* identity_manager);
  ~ChromeTailoredSecurityService() override;

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

 private:
  ChromeBrowserState* browser_state_;
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_
