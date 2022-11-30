// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"

#import <ostream>

#import "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromeIdentityInteractionManager

- (void)addAccountWithPresentingViewController:(UIViewController*)viewController
                                     userEmail:(NSString*)userEmail
                                    completion:
                                        (SigninCompletionCallback)completion {
  NOTREACHED() << "Subclasses must override this";
}

- (void)cancelAddAccountAnimated:(BOOL)animated
                      completion:(ProceduralBlock)completion {
  NOTREACHED() << "Subclasses must override this";
}

@end
