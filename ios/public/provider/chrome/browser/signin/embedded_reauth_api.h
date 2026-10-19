// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_EMBEDDED_REAUTH_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_EMBEDDED_REAUTH_API_H_

#import <memory>

#import "ios/public/provider/chrome/browser/signin/embedded_reauth_interaction_manager.h"

namespace ios::provider {

// Creates an instance of `EmbeddedReAuthInteractionManager`.
std::unique_ptr<EmbeddedReAuthInteractionManager>
CreateEmbeddedReAuthInteractionManager();

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_EMBEDDED_REAUTH_API_H_
