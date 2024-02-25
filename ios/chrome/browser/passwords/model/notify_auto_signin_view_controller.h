// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_NOTIFY_AUTO_SIGNIN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_NOTIFY_AUTO_SIGNIN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class GURL;

// UIViewController for the notification about auto sign-in.
@interface NotifyUserAutoSigninViewController : UIViewController

- (instancetype)initWithUsername:(NSString*)username
                         iconURL:(GURL)iconURL
                URLLoaderFactory:
                    (scoped_refptr<network::SharedURLLoaderFactory>)
                        URLLoaderFactory NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_NOTIFY_AUTO_SIGNIN_VIEW_CONTROLLER_H_
