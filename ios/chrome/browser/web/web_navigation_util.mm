// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/web_navigation_util.h"

#import <Foundation/Foundation.h>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web_navigation_util {

web::NavigationManager::WebLoadParams CreateWebLoadParams(
    const GURL& URL,
    ui::PageTransition transition_type,
    TemplateURLRef::PostContent* post_data) {
  web::NavigationManager::WebLoadParams params(URL);
  params.transition_type = transition_type;
  if (post_data) {
    // Extract the content type and post params from |postData| and add them
    // to the load params.
    NSString* contentType = base::SysUTF8ToNSString(post_data->first);
    NSData* data = [NSData dataWithBytes:(void*)post_data->second.data()
                                  length:post_data->second.length()];
    params.post_data = data;
    params.extra_headers = @{@"Content-Type" : contentType};
  }
  return params;
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
