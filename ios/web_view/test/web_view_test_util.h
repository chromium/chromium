// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_TEST_WEB_VIEW_TEST_UTIL_H_
#define IOS_WEB_VIEW_TEST_WEB_VIEW_TEST_UTIL_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CWVWebView;

namespace ios_web_view {
namespace test {

// Creates web view with default configuration and frame equal to screen bounds.
[[nodiscard]] CWVWebView* CreateWebView();

// Loads |URL| in |web_view| and waits until the load completes. Asserts if
// loading does not complete.
[[nodiscard]] bool LoadUrl(CWVWebView* web_view, NSURL* url);

// Returns whether the element with |element_id| in the passed |web_view| has
// been tapped using a JavaScript click() event.
[[nodiscard]] bool TapWebViewElementWithId(CWVWebView* web_view,
                                           NSString* element_id);

// Waits until |script| is executed and synchronously returns the evaluation
// result.
id EvaluateJavaScript(CWVWebView* web_view,
                      NSString* script,
                      NSError** error = nil);

// Waits for |web_view| to contain |text|. Returns false if the condition is not
// met within a timeout.
[[nodiscard]] bool WaitForWebViewContainingTextOrTimeout(CWVWebView* web_view,
                                                         NSString* text);

// Waits until |web_view| stops loading. Returns false if the condition is not
// met within a timeout.
[[nodiscard]] bool WaitForWebViewLoadCompletionOrTimeout(CWVWebView* web_view);

// Copies the state of |source_web_view| to |destination_web_view| using state
// restoration.
void CopyWebViewState(CWVWebView* source_web_view,
                      CWVWebView* destination_web_view);

}  // namespace test
}  // namespace ios_web_view

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_TEST_WEB_VIEW_TEST_UTIL_H_
