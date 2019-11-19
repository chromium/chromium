// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_PAGE_SCRIPT_UTIL_H_
#define IOS_WEB_JS_MESSAGING_PAGE_SCRIPT_UTIL_H_

#import <Foundation/Foundation.h>

namespace web {

class BrowserState;

// Returns an autoreleased string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name);

// Returns an autoreleased string containing the JavaScript to be injected into
// the main frame of the web view as early as possible.
NSString* GetDocumentStartScriptForMainFrame(BrowserState* browser_state);

// Returns an autoreleased string containing the JavaScript to be injected into
// the main frame of the web view at the end of the document load.
NSString* GetDocumentEndScriptForMainFrame(BrowserState* browser_state);

// Returns an autoreleased string containing the JavaScript to be injected into
// all frames of the web view as early as possible.
NSString* GetDocumentStartScriptForAllFrames(BrowserState* browser_state);

// Returns an autoreleased string containing the JavaScript to be injected into
// all frames of the web view at the end of the document load.
NSString* GetDocumentEndScriptForAllFrames(BrowserState* browser_state);

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_PAGE_SCRIPT_UTIL_H_
