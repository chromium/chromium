// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_JS_TEST_H_
#define IOS_WEB_PUBLIC_TEST_WEB_JS_TEST_H_

#import <Foundation/Foundation.h>

#include <memory>

#import "base/mac/bundle_locations.h"
#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/web_client.h"
#import "testing/gtest_mac.h"

namespace web {

// Base fixture mixin for testing JavaScripts.
template <class WebTestT>
class WebJsTest : public WebTestT {
 public:
  WebJsTest(NSArray* java_script_paths)
      : java_script_paths_([java_script_paths copy]) {}
  WebJsTest(std::unique_ptr<web::WebClient> web_client)
      : WebTestT(std::move(web_client)) {}

 protected:
  // Loads |html| and inject JavaScripts at |javaScriptPaths_|.
  void LoadHtmlAndInject(NSString* html) {
    WebTestT::LoadHtml(html);
    Inject();
  }
  void LoadHtmlAndInject(NSString* html, const GURL& url) {
    WebTestT::LoadHtml(html, url);
    Inject();
  }

  // Helper method that EXPECTs the |java_script| evaluation results on each
  // element obtained by scripts in |get_element_javas_cripts|; the expected
  // result is the corresponding entry in |expected_results|.
  void ExecuteJavaScriptOnElementsAndCheck(NSString* java_script,
                                           NSArray* get_element_java_scripts,
                                           NSArray* expected_results);

  // Helper method that EXPECTs the |java_script| evaluation results on each
  // element obtained by JavaScripts in |get_element_java_scripts|. The
  // expected results are boolean and are true only for elements in
  // |get_element_java_scripts_expecting_true| which is subset of
  // |get_element_java_scripts|.
  void ExecuteBooleanJavaScriptOnElementsAndCheck(
      NSString* java_script,
      NSArray* get_element_java_scripts,
      NSArray* get_element_java_scripts_expecting_true);

 private:
  // Injects JavaScript at |java_script_paths_|.
  void Inject();

  NSArray* java_script_paths_;
};

template <class WebTestT>
void WebJsTest<WebTestT>::Inject() {
  // Main web injection should have occurred.
  ASSERT_NSEQ(@"object", WebTestT::ExecuteJavaScript(@"typeof __gCrWeb"));

  for (NSString* java_script_path in java_script_paths_) {
    WebTestT::ExecuteJavaScript(web::test::GetPageScript(java_script_path));
  }
}

template <class WebTestT>
void WebJsTest<WebTestT>::ExecuteJavaScriptOnElementsAndCheck(
    NSString* java_script,
    NSArray* get_element_java_scripts,
    NSArray* expected_results) {
  for (NSUInteger i = 0; i < get_element_java_scripts.count; ++i) {
    NSString* js_to_execute =
        [NSString stringWithFormat:java_script, get_element_java_scripts[i]];
    EXPECT_NSEQ(expected_results[i],
                WebTestT::ExecuteJavaScript(js_to_execute));
  }
}

template <class WebTestT>
void WebJsTest<WebTestT>::ExecuteBooleanJavaScriptOnElementsAndCheck(
    NSString* java_script,
    NSArray* get_element_java_scripts,
    NSArray* get_element_java_scripts_expecting_true) {
  for (NSString* get_element_java_script in get_element_java_scripts) {
    NSString* js_to_execute =
        [NSString stringWithFormat:java_script, get_element_java_script];
    BOOL expected = [get_element_java_scripts_expecting_true
        containsObject:get_element_java_script];
    EXPECT_NSEQ(@(expected), WebTestT::ExecuteJavaScript(js_to_execute))
        << [NSString stringWithFormat:@"%@ on %@ should return %d", java_script,
                                      get_element_java_script, expected];
  }
}

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_JS_TEST_H_
