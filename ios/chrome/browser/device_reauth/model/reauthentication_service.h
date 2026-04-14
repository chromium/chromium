// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_REAUTHENTICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_REAUTHENTICATION_SERVICE_H_

#import <Foundation/Foundation.h>

#import "components/keyed_service/core/keyed_service.h"

@protocol ReauthenticationProtocol;

// Owns the reauthentication module that should be reused for a profile.
class ReauthenticationService : public KeyedService {
 public:
  explicit ReauthenticationService(id<ReauthenticationProtocol> reauth_module);
  ~ReauthenticationService() override;

  // KeyedService implementation.
  void Shutdown() override;

  id<ReauthenticationProtocol> GetReauthModule();

 private:
  id<ReauthenticationProtocol> reauth_module_ = nil;
};

#endif  // IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_REAUTHENTICATION_SERVICE_H_
