// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_view_content_test_util.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/containers/contains.h"
#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

using base::test::ios::kWaitForDownloadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// A helper delegate class that allows downloading responses with invalid
// SSL certs.
@interface TestURLSessionDelegate : NSObject <NSURLSessionDelegate>
@end

@implementation TestURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential*))completionHandler {
  SecTrustRef serverTrust = challenge.protectionSpace.serverTrust;
  completionHandler(NSURLSessionAuthChallengeUseCredential,
                    [NSURLCredential credentialForTrust:serverTrust]);
}

@end

namespace {
// Script that returns the document contents as a string.
char kGetDocumentBodyJavaScript[] =
    "function allTextContent(element) { "
    "  if (!element) { return ''; }"
    "  let textString = element.textContent;"
    "  if (element == document.body || element instanceof HTMLElement) {"
    "    for (let e of element.getElementsByTagName('*')) {"
    "      if (e && e.shadowRoot) {"
    "        textString += '|' + allTextContent(e.shadowRoot);"
    "      }"
    "    }"
    "  }"
    "  return textString;"
    "}"
    "allTextContent(document.body);";

// Fetches the image from `image_url`.
UIImage* LoadImage(const GURL& image_url) {
  __block UIImage* image;
  __block NSError* error;
  TestURLSessionDelegate* session_delegate =
      [[TestURLSessionDelegate alloc] init];
  NSURLSessionConfiguration* session_config =
      [NSURLSessionConfiguration ephemeralSessionConfiguration];
  NSURLSession* session =
      [NSURLSession sessionWithConfiguration:session_config
                                    delegate:session_delegate
                               delegateQueue:nil];
  id completion_handler = ^(NSData* data, NSURLResponse*, NSError* task_error) {
    error = task_error;
    image = [[UIImage alloc] initWithData:data];
  };

  NSURLSessionDataTask* task =
      [session dataTaskWithURL:net::NSURLWithGURL(image_url)
             completionHandler:completion_handler];
  [task resume];

  bool task_completed = WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return image || error;
  });

  if (!task_completed) {
    return nil;
  }
  return image;
}
}

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForUIElementTimeout;

namespace web {
namespace test {

bool IsWebViewContainingText(web::WebState* web_state,
                             const std::string& text) {
  std::unique_ptr<base::Value> value =
      web::test::ExecuteJavaScript(web_state, kGetDocumentBodyJavaScript);
  std::string body;
  if (value && value->is_string()) {
    return base::Contains(value->GetString(), text);
  }
  return false;
}

bool IsWebViewContainingTextInFrame(web::WebState* web_state,
                                    const std::string& text) {
  __block NSInteger number_frames_processing = 0;
  __block bool text_found = false;
  for (WebFrame* frame :
       web_state->GetPageWorldWebFramesManager()->GetAllWebFrames()) {
    number_frames_processing++;

    FindInPageJavaScriptFeature* find_in_page_feature =
        FindInPageJavaScriptFeature::GetInstance();
    find_in_page_feature->Search(
        frame, text, base::BindOnce(^(std::optional<int> result_matches) {
          if (result_matches && result_matches.value() >= 1) {
            text_found = true;
          }
          number_frames_processing--;
        }));
  }
  bool success = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    if (text_found)
      return true;
    return number_frames_processing == 0;
  });
  return text_found && success;
}

bool WaitForWebViewContainingText(web::WebState* web_state,
                                  std::string text,
                                  base::TimeDelta timeout) {
  return WaitUntilConditionOrTimeout(timeout, ^{
    base::RunLoop().RunUntilIdle();
    return IsWebViewContainingText(web_state, text);
  });
}

bool WaitForWebViewNotContainingText(web::WebState* web_state,
                                     std::string text,
                                     base::TimeDelta timeout) {
  return WaitUntilConditionOrTimeout(timeout, ^{
    base::RunLoop().RunUntilIdle();
    return !IsWebViewContainingText(web_state, text);
  });
}

bool WaitForWebViewContainingTextInFrame(web::WebState* web_state,
                                         std::string text,
                                         base::TimeDelta timeout) {
  return WaitUntilConditionOrTimeout(timeout, ^{
    base::RunLoop().RunUntilIdle();
    return IsWebViewContainingTextInFrame(web_state, text);
  });
}

bool WaitForWebViewContainingImage(std::string image_id,
                                   web::WebState* web_state,
                                   ImageStateElement image_state) {
  std::string get_url_script =
      base::StringPrintf("document.getElementById('%s').src", image_id.c_str());
  std::unique_ptr<base::Value> url_as_value =
      web::test::ExecuteJavaScript(web_state, get_url_script);
  if (!url_as_value->is_string())
    return false;

  UIImage* image = LoadImage(GURL(url_as_value->GetString()));
  if (!image)
    return false;

  CGSize expected_size = image.size;

  return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    NSString* const kGetElementAttributesScript =
        [NSString stringWithFormat:@"var image = document.getElementById('%@');"
                                   @"var imageHeight = image.height;"
                                   @"var imageWidth = image.width;"
                                   @"JSON.stringify({"
                                   @"  height:imageHeight,"
                                   @"  width:imageWidth"
                                   @"});",
                                   base::SysUTF8ToNSString(image_id)];
    std::unique_ptr<base::Value> value = web::test::ExecuteJavaScript(
        web_state, base::SysNSStringToUTF8(kGetElementAttributesScript));
    if (value && value->is_string()) {
      NSString* evaluation_result = base::SysUTF8ToNSString(value->GetString());
      NSData* image_attributes_as_data =
          [evaluation_result dataUsingEncoding:NSUTF8StringEncoding];
      NSDictionary* image_attributes =
          [NSJSONSerialization JSONObjectWithData:image_attributes_as_data
                                          options:0
                                            error:nil];
      CGFloat height = [image_attributes[@"height"] floatValue];
      CGFloat width = [image_attributes[@"width"] floatValue];
      switch (image_state) {
        case IMAGE_STATE_BLOCKED:
          return height < expected_size.height && width < expected_size.width;
        case IMAGE_STATE_LOADED:
          return height == expected_size.height && width == expected_size.width;
      }
    }
    return false;
  });
}

bool IsWebViewContainingElement(web::WebState* web_state,
                                ElementSelector* selector) {
  // Script that tests presence of element.
  std::string script = base::SysNSStringToUTF8(
      [NSString stringWithFormat:@"!!(%@)", selector.selectorScript]);

  std::unique_ptr<base::Value> value =
      web::test::ExecuteJavaScript(web_state, script);
  if (!value)
    return false;
  return value->GetIfBool().value_or(false);
}

bool WaitForWebViewContainingElement(web::WebState* web_state,
                                     ElementSelector* selector) {
  return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return IsWebViewContainingElement(web_state, selector);
  });
}

bool WaitForWebViewNotContainingElement(web::WebState* web_state,
                                        ElementSelector* selector) {
  return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !IsWebViewContainingElement(web_state, selector);
  });
}

}  // namespace test
}  // namespace web
