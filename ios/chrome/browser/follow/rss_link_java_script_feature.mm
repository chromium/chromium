// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/rss_link_java_script_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "ios/chrome/browser/follow/rss_link_java_script_feature.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "ios/web/public/js_messaging/web_frame.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kRSSLinkScript[] = "rss_link_js";
const char kGetRSSLinkFunction[] = "rssLink.getRSSLinks";
// The timeout for any JavaScript call in this file.
const double kJavaScriptExecutionTimeoutInMs = 500.0;

}  // namespace

// static
RSSLinkJavaScriptFeature* RSSLinkJavaScriptFeature::GetInstance() {
  static base::NoDestructor<RSSLinkJavaScriptFeature> instance;
  return instance.get();
}

RSSLinkJavaScriptFeature::RSSLinkJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kAnyContentWorld,
          {FeatureScript::CreateWithFilename(
              kRSSLinkScript,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}),
      weak_ptr_factory_(this) {}

RSSLinkJavaScriptFeature::~RSSLinkJavaScriptFeature() = default;

void RSSLinkJavaScriptFeature::GetRSSLinks(
    web::WebFrame* frame,
    base::OnceCallback<void(std::vector<std::string>*)> callback) {
  CallJavaScriptFunction(
      frame, kGetRSSLinkFunction, /* parameters= */ {},
      base::BindOnce(&RSSLinkJavaScriptFeature::HandleResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      base::Milliseconds(kJavaScriptExecutionTimeoutInMs));
}

void RSSLinkJavaScriptFeature::HandleResponse(
    base::OnceCallback<void(std::vector<std::string>*)> callback,
    const base::Value* response) {
  if (!response)
    return;

  if (!response->is_list())
    return;

  std::vector<std::string> rss_links;
  // Iterate through all the extracted links and copy
  // the data from JSON.
  for (const auto& link : response->GetList()) {
    rss_links.push_back(*link.GetIfString());
  }
  std::move(callback).Run(&rss_links);
}
