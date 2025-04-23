// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_MANAGEMENT_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_MANAGEMENT_SERVICE_IOS_H_

#import "components/keyed_service/core/keyed_service.h"
#import "components/policy/core/common/management/management_service.h"

class ProfileIOS;

namespace policy {

// This class gives information related to the browser's management state.
// For more information please read
// //components/policy/core/common/management/management_service.md
class ManagementServiceIOS : public ManagementService, public KeyedService {
 public:
  explicit ManagementServiceIOS(ProfileIOS* profile);
  ~ManagementServiceIOS() override;
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_MANAGEMENT_SERVICE_IOS_H_
