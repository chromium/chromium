// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_SELECTION_WEB_SELECTION_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_WEB_SELECTION_WEB_SELECTION_JAVA_SCRIPT_FEATURE_H_

#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebState;
}  // namespace web

@class WebSelectionResponse;

/**
 * Handles JS communication to retrieve the page selection.
 */
class WebSelectionJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static WebSelectionJavaScriptFeature* GetInstance();

  WebSelectionJavaScriptFeature(const WebSelectionJavaScriptFeature&) = delete;
  WebSelectionJavaScriptFeature& operator=(
      const WebSelectionJavaScriptFeature&) = delete;

  // Invokes JS-side handlers to grab the current selected text and returns it
  // with its bounding box in the page.
  virtual void GetSelectedText(
      web::WebState* web_state,
      base::OnceCallback<void(WebSelectionResponse*)> callback);

 private:
  WebSelectionJavaScriptFeature();
  ~WebSelectionJavaScriptFeature() override;

  friend class base::NoDestructor<WebSelectionJavaScriptFeature>;

  void HandleResponse(
      base::WeakPtr<web::WebState> weak_web_state,
      base::OnceCallback<void(WebSelectionResponse*)> final_callback,
      const base::Value* response);
  void ProcessResponseFromSubframes(
      base::OnceCallback<void(WebSelectionResponse*)> final_callback,
      std::vector<WebSelectionResponse*> responses);
  void RunGetSelectionFunction(
      web::WebFrame* frame,
      base::OnceCallback<void(const base::Value*)> callback);

  base::WeakPtrFactory<WebSelectionJavaScriptFeature> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_WEB_SELECTION_WEB_SELECTION_JAVA_SCRIPT_FEATURE_H_
