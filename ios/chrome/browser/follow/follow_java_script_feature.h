// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_FOLLOW_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_FOLLOW_FOLLOW_JAVA_SCRIPT_FEATURE_H_

#import "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/values.h"
#import "ios/chrome/browser/ui/follow/follow_site_info.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#include "url/gurl.h"

/**
 * Handles JS communication for the following feed feature.
 */
class FollowJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static FollowJavaScriptFeature* GetInstance();

  // Invokes JS-side handlers to get the website information.
  virtual void GetFollowSiteInfo(
      web::WebState* web_state,
      base::OnceCallback<void(FollowSiteInfo*)> callback);

 private:
  friend class base::NoDestructor<FollowJavaScriptFeature>;

  FollowJavaScriptFeature();
  ~FollowJavaScriptFeature() override;

  void HandleResponse(const GURL& url,
                      base::OnceCallback<void(FollowSiteInfo*)> callback,
                      const base::Value* response);

  FollowJavaScriptFeature(const FollowJavaScriptFeature&) = delete;
  FollowJavaScriptFeature& operator=(const FollowJavaScriptFeature&) = delete;

  base::WeakPtrFactory<FollowJavaScriptFeature> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_FOLLOW_FOLLOW_JAVA_SCRIPT_FEATURE_H_
