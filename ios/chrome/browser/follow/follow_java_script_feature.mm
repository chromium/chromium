// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/follow_java_script_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#include "ios/web/public/web_state.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kRSSLinkScript[] = "rss_link";
const char kGetRSSLinkFunction[] = "rssLink.getRSSLinks";
// The timeout for any JavaScript call in this file.
const double kJavaScriptExecutionTimeoutInMs = 500.0;

}  // namespace

// static
FollowJavaScriptFeature* FollowJavaScriptFeature::GetInstance() {
  static base::NoDestructor<FollowJavaScriptFeature> instance;
  return instance.get();
}

FollowJavaScriptFeature::FollowJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kAnyContentWorld,
          {FeatureScript::CreateWithFilename(
              kRSSLinkScript,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}),
      weak_ptr_factory_(this) {}

FollowJavaScriptFeature::~FollowJavaScriptFeature() = default;

void FollowJavaScriptFeature::GetFollowWebPageURLs(
    web::WebState* web_state,
    base::OnceCallback<void(FollowWebPageURLs*)> callback) {
  if (!web::GetMainFrame(web_state)) {
    std::move(callback).Run(nil);
    return;
  }
  CallJavaScriptFunction(
      web::GetMainFrame(web_state), kGetRSSLinkFunction, /* parameters= */ {},
      base::BindOnce(&FollowJavaScriptFeature::HandleResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     web_state->GetLastCommittedURL(), std::move(callback)),
      base::Milliseconds(kJavaScriptExecutionTimeoutInMs));
}

void FollowJavaScriptFeature::HandleResponse(
    const GURL& url,
    base::OnceCallback<void(FollowWebPageURLs*)> callback,
    const base::Value* response) {
  if (response && response->is_list()) {
    NSMutableArray* rss_links = [[NSMutableArray alloc] init];
    for (const auto& link : response->GetListDeprecated()) {
      if (link.is_string()) {
        [rss_links addObject:[NSURL URLWithString:base::SysUTF8ToNSString(
                                                      *link.GetIfString())]];
      }
    }
    std::move(callback).Run([[FollowWebPageURLs alloc]
        initWithWebPageURL:net::NSURLWithGURL(url)
                  RSSLinks:rss_links]);
    return;
  }

  std::move(callback).Run([[FollowWebPageURLs alloc]
      initWithWebPageURL:net::NSURLWithGURL(url)
                RSSLinks:nil]);
}
