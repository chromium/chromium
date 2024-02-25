// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/model/follow_java_script_feature.h"

#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"

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
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kRSSLinkScript,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}),
      weak_ptr_factory_(this) {}

FollowJavaScriptFeature::~FollowJavaScriptFeature() = default;

void FollowJavaScriptFeature::GetWebPageURLs(web::WebState* web_state,
                                             ResultCallback callback) {
  web::WebFrame* main_frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  if (!main_frame) {
    std::move(callback).Run(nil);
    return;
  }
  CallJavaScriptFunction(
      main_frame, kGetRSSLinkFunction,
      /* parameters= */ {},
      base::BindOnce(&FollowJavaScriptFeature::HandleResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     web_state->GetLastCommittedURL(), std::move(callback)),
      base::Milliseconds(kJavaScriptExecutionTimeoutInMs));
}

void FollowJavaScriptFeature::HandleResponse(const GURL& url,
                                             ResultCallback callback,
                                             const base::Value* response) {
  NSMutableArray<NSURL*>* rss_urls = nil;
  if (response && response->is_list()) {
    for (const auto& link : response->GetList()) {
      if (link.is_string()) {
        NSURL* nsurl = net::NSURLWithGURL(GURL(link.GetString()));
        if (nsurl) {
          if (!rss_urls) {
            rss_urls = [[NSMutableArray alloc] init];
          }

          [rss_urls addObject:nsurl];
        }
      }
    }
  }

  WebPageURLs* web_page_urls =
      [[WebPageURLs alloc] initWithURL:net::NSURLWithGURL(url)
                               RSSURLs:rss_urls];

  std::move(callback).Run(web_page_urls);
}
