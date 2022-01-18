// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_RSS_LINK_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_FOLLOW_RSS_LINK_JAVA_SCRIPT_FEATURE_H_

#include <string>
#include <vector>

#import "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

/**
 * Handles JS communication for the web channels feature.
 */
class RSSLinkJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static RSSLinkJavaScriptFeature* GetInstance();

  // Invokes JS-side handlers to get the RSS links.
  virtual void GetRSSLinks(
      web::WebFrame* frame,
      base::OnceCallback<void(std::vector<std::string>*)> callback);

 private:
  friend class base::NoDestructor<RSSLinkJavaScriptFeature>;

  RSSLinkJavaScriptFeature();
  ~RSSLinkJavaScriptFeature() override;

  void HandleResponse(
      base::OnceCallback<void(std::vector<std::string>*)> callback,
      const base::Value* response);

  RSSLinkJavaScriptFeature(const RSSLinkJavaScriptFeature&) = delete;
  RSSLinkJavaScriptFeature& operator=(const RSSLinkJavaScriptFeature&) = delete;

  base::WeakPtrFactory<RSSLinkJavaScriptFeature> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_FOLLOW_RSS_LINK_JAVA_SCRIPT_FEATURE_H_
