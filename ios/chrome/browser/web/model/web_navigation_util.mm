// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_navigation_util.h"

#import <Foundation/Foundation.h>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/variations/net/variations_http_headers.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/resource_request.h"
#import "url/gurl.h"

namespace web_navigation_util {

web::NavigationManager::WebLoadParams CreateWebLoadParams(
    const GURL& URL,
    ui::PageTransition transition_type,
    TemplateURLRef::PostContent* post_data) {
  web::NavigationManager::WebLoadParams params(URL);
  params.transition_type = transition_type;
  if (post_data) {
    // Extract the content type and post params from `postData` and add them
    // to the load params.
    NSString* contentType = base::SysUTF8ToNSString(post_data->first);
    NSData* data = [NSData dataWithBytes:(void*)post_data->second.data()
                                  length:post_data->second.length()];
    params.post_data = data;
    params.extra_headers = @{@"Content-Type" : contentType};
  }
  return params;
}

NSDictionary<NSString*, NSString*>* VariationHeadersForURL(const GURL& url,
                                                           bool is_incognito) {
  NSMutableDictionary* result = [NSMutableDictionary dictionary];

  network::ResourceRequest resource_request;
  if (!variations::AppendVariationsHeaderUnknownSignedIn(
          url,
          is_incognito ? variations::InIncognito::kYes
                       : variations::InIncognito::kNo,
          &resource_request)) {
    // AppendVariationsHeaderUnknownSignedIn returns NO if custom headers
    // were not added. In that case, return an empty dictionary.
    return @{};
  }
  // The variations header appears in cors_exempt_headers rather than in
  // headers.
  net::HttpRequestHeaders::Iterator header_iterator(
      resource_request.cors_exempt_headers);
  while (header_iterator.GetNext()) {
    NSString* name = base::SysUTF8ToNSString(header_iterator.name());
    NSString* value = base::SysUTF8ToNSString(header_iterator.value());
    result[name] = value;
  }
  return [result copy];
}

void GoBack(web::WebState* web_state) {
  DCHECK(web_state);
  base::RecordAction(base::UserMetricsAction("Back"));
  web_state->GetNavigationManager()->GoBack();
}

void GoForward(web::WebState* web_state) {
  DCHECK(web_state);
  base::RecordAction(base::UserMetricsAction("Forward"));
  web_state->GetNavigationManager()->GoForward();
}

}  // namespace web_navigation_util
