// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_PAGE_SCRIPT_UTIL_H_
#define IOS_WEB_JS_MESSAGING_PAGE_SCRIPT_UTIL_H_

#import <Foundation/Foundation.h>

namespace web {

// Returns an autoreleased string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name);

// Make sure that script is injected only once. For example, content of
// WKUserScript can be injected into the same page multiple times
// without notifying WKNavigationDelegate (e.g. after window.document.write
// JavaScript call). Injecting the script multiple times invalidates the
// web frame id and will break the ability to send messages from JS to the
// native code. Wrapping injected script into "if (!injected)" check prevents
// multiple injections into the same page. `script_identifier` should identify
// the script being injected in order to enforce the injection of `script` to
// only once.
// NOTE: `script_identifier` will be used as the suffix for a JavaScript var, so
// it must adhere to JavaScript var naming rules.
NSString* MakeScriptInjectableOnce(NSString* script_identifier,
                                   NSString* script);

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_PAGE_SCRIPT_UTIL_H_
