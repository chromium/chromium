// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_RESOURCES_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_RESOURCES_PROVIDER_H_


#include "base/macros.h"

@class NSString;
@class UIImage;

namespace ios {

enum class SigninStringID {
  BUTTON_CANCEL,
  REMOVE_ACCOUNT,
  REMOVE_ACCOUNT_CONFIRMATION,
  UNNAMED_ACCOUNT,
};

class SigninResourcesProvider {
 public:
  SigninResourcesProvider();
  virtual ~SigninResourcesProvider();

  // Returns a default avatar to use when the identity is missing one.
  virtual UIImage* GetDefaultAvatar();

  // Returns a localized string.
  virtual NSString* GetLocalizedString(SigninStringID string_id);

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninResourcesProvider);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_RESOURCES_PROVIDER_H_
