// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/gaia_auth_fetcher_ios_bridge.h"

#import <Foundation/Foundation.h>

#pragma mark - GaiaAuthFetcherIOSBridge::Delegate

GaiaAuthFetcherIOSBridge::Delegate::Delegate() {}

GaiaAuthFetcherIOSBridge::Delegate::~Delegate() {}

#pragma mark - GaiaAuthFetcherIOSBridge

GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridge(Delegate* delegate)
    : delegate_(delegate) {}

GaiaAuthFetcherIOSBridge::~GaiaAuthFetcherIOSBridge() {}
