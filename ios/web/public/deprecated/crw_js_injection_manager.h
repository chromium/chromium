// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DEPRECATED_CRW_JS_INJECTION_MANAGER_H_
#define IOS_WEB_PUBLIC_DEPRECATED_CRW_JS_INJECTION_MANAGER_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/deprecated/crw_js_injection_evaluator.h"

@class CRWJSInjectionReceiver;

// This class defines the abstract interface for managing JavaScript
// Injection into a UIWebView.
@interface CRWJSInjectionManager : NSObject

// Designated initializer. Initializes the object with the |receiver|.
- (id)initWithReceiver:(CRWJSInjectionReceiver*)receiver;

// Returns whether JavaScript has already been injected into the receiver.
- (BOOL)hasBeenInjected;

// Injects JavaScript at |self.scriptPath| into the receiver object if it is
// missing. It also injects the dependencies' JavaScript if they are missing.
- (void)inject;

// Executes the supplied JavaScript asynchronously. Calls |completionHandler|
// with results of the execution (which may be nil) or an NSError if there is an
// error. The |completionHandler| can be nil.
- (void)executeJavaScript:(NSString*)script
        completionHandler:(void (^)(id, NSError*))completionHandler;

@end

@interface CRWJSInjectionManager (ProtectedMethods)

// The injection receiver used to evaluate JavaScript.
- (CRWJSInjectionReceiver*)receiver;

// Path for the resource in the application bundle of type "js" that needs to
// injected for this manager.
// Subclasses must override this method to return the path to the JavaScript
// that needs to be injected.
- (NSString*)scriptPath;

// Returns the content that should be injected. This is called every time
// injection content is needed; by default is uses a cached copy of
// staticInjectionContent.
// Subclasses should override this only if the content needs to be dynamic
// rather than cached, otherwise they should override staticInjectionContent.
- (NSString*)injectionContent;

// Returns an autoreleased string that is the JavaScript to be injected into
// the receiver object. By default this returns the contents of the script file;
// subclasses can override this if they need to get a static script from some
// other source.
// The return value from this method will be cached; if dynamic script content
// is necessary, override injectionContent instead.
- (NSString*)staticInjectionContent;

@end

#endif  // IOS_WEB_PUBLIC_DEPRECATED_CRW_JS_INJECTION_MANAGER_H_
