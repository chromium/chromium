// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_CONFIGURATION_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <memory>

class AuthenticationService;

namespace ios::provider {
enum class BWGPageContextState;
}  // namespace ios::provider

namespace optimization_guide::proto {
class PageContext;
}  // namespace optimization_guide::proto

// BWGConfiguration is a configuration class that holds all the data necessary
// to start the BWG overlay.
@interface BWGConfiguration : NSObject

// The base view controller to present the UI on.
@property(nonatomic, weak) UIViewController* baseViewController;

// The PageContext for the current WebState. This is a unique_ptr, so subsequent
// calls to the getter will return a nullptr.
@property(nonatomic, assign)
    std::unique_ptr<optimization_guide::proto::PageContext>
        pageContext;

// The authentication service to be used.
@property(nonatomic, assign) AuthenticationService* authService;

// The state of the BWG PageContext.
@property(nonatomic, assign)
    ios::provider::BWGPageContextState BWGPageContextState;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_CONFIGURATION_H_
