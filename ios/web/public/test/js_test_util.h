// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_JS_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_JS_TEST_UTIL_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import <vector>

namespace web {

class BrowserState;
class JavaScriptFeature;
class WebState;

namespace test {

// These functions synchronously execute `script` in `web_view` and wait for its
// completion. Prefer these variants over those below returning `id` when
// the caller is not interested in the return value.
void ExecuteJavaScriptInWebView(WKWebView* web_view, NSString* script);
void ExecuteJavaScriptInWebView(WKWebView* web_view,
                                NSString* script,
                                NSError* __autoreleasing* error);

// Synchronously executes JavaScript in the content world associated with
// `feature`.
void ExecuteJavaScriptForFeature(web::WebState* web_state,
                                 NSString* script,
                                 JavaScriptFeature* feature);

// These functions synchronously execute JavaScript and return result as id.
// id will be backed up by different classes depending on resulting JS type:
// NSString (string), NSNumber (number or boolean), NSDictionary (object),
// NSArray (array), NSNull (null), NSDate (Date), nil (undefined or execution
// exception).

// Executes JavaScript on `web_view` and returns the result as an id.
// `error` can be null and will be updated only if script execution fails.
[[nodiscard]] id ExecuteJavaScript(WKWebView* web_view,
                                   NSString* script,
                                   NSError* __autoreleasing* error);

// Executes JavaScript on `web_view` and returns the result as an id.
// Fails if there was an error during script execution.
[[nodiscard]] id ExecuteJavaScript(WKWebView* web_view, NSString* script);

// Synchronously executes JavaScript in the content world associated with
// `feature` and returns the result as id.
[[nodiscard]] id ExecuteJavaScriptForFeatureAndReturnResult(
    web::WebState* web_state,
    NSString* script,
    JavaScriptFeature* feature);

// Synchronously loads `html` into `web_view`. Returns true is successful or
// false if the `web_view` never finishes loading.
[[nodiscard]] bool LoadHtml(WKWebView* web_view,
                            NSString* html,
                            NSURL* base_url);

// Waits until custom javascript is injected into __gCrWeb.
[[nodiscard]] bool WaitForInjectedScripts(WKWebView* web_view);

// Returns an autoreleased string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name);

// Manually overrides the built in JavaScriptFeatures and those from
// `GetWebClient()::GetJavaScriptFeatures()`. This is intended to be used to
// replace an instance of a built in feature with one created by the test.
// NOTE: Do not call this when using a WebClient with features you rely on or
// `FakeWebClient::SetJavaScriptFeatures` as this will override those
// features.
void OverrideJavaScriptFeatures(web::BrowserState* browser_state,
                                std::vector<JavaScriptFeature*> features);

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_JS_TEST_UTIL_H_
