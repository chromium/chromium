// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_CWV_TRUSTED_VAULT_OBSERVER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_CWV_TRUSTED_VAULT_OBSERVER_INTERNAL_H_

#include "components/trusted_vault/trusted_vault_client.h"
#import "ios/web_view/public/cwv_trusted_vault_observer.h"

@interface CWVTrustedVaultObserver ()

- (instancetype)initWithTrustedVaultObserver:
    (trusted_vault::TrustedVaultClient::Observer*)observer
    NS_DESIGNATED_INITIALIZER;

@property(nonatomic, readonly)
    trusted_vault::TrustedVaultClient::Observer* observer;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_CWV_TRUSTED_VAULT_OBSERVER_INTERNAL_H_
