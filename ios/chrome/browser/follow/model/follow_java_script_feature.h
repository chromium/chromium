// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_JAVA_SCRIPT_FEATURE_H_

#import "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/values.h"
#import "ios/chrome/browser/follow/model/web_page_urls.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#include "url/gurl.h"

/**
 * Handles JS communication for the following feed feature.
 */
class FollowJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static FollowJavaScriptFeature* GetInstance();

  // Callback invoked when the URLs identifying the website have
  // been extracted from the page.
  using ResultCallback = base::OnceCallback<void(WebPageURLs* web_page_urls)>;

  // Invokes JS-side handlers to get the webpage information.
  virtual void GetWebPageURLs(web::WebState* web_state,
                              ResultCallback callback);

 private:
  friend class base::NoDestructor<FollowJavaScriptFeature>;

  FollowJavaScriptFeature();
  ~FollowJavaScriptFeature() override;

  void HandleResponse(const GURL& url,
                      ResultCallback callback,
                      const base::Value* response);

  FollowJavaScriptFeature(const FollowJavaScriptFeature&) = delete;
  FollowJavaScriptFeature& operator=(const FollowJavaScriptFeature&) = delete;

  base::WeakPtrFactory<FollowJavaScriptFeature> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_JAVA_SCRIPT_FEATURE_H_
