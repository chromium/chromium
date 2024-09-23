// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_SHARE_TO_DATA_BUILDER_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_SHARE_TO_DATA_BUILDER_H_

#import <UIKit/UIKit.h>

class GURL;

@class ShareToData;
@class URLWithTitle;

namespace web {
class WebState;
}

namespace activity_services {

// Returns a ShareToData object using data from `web_state`. `share_url` is the
// URL to be shared with share extensions. If `share_url` is empty, the visible
// URL associated with `web_state` will be used instead. `web_state` must not be
// nil. Function may return nil.
ShareToData* ShareToDataForWebState(web::WebState* web_state,
                                    const GURL& share_url);

// Returns a ShareToData object for a single `url`, and its page's `title`,
// which is not associated to a WebState. Will also add `additional_text`, if
// present.
ShareToData* ShareToDataForURL(const GURL& url,
                               NSString* title,
                               NSString* additional_text,
                               LPLinkMetadata* link_metadata);

// Returns a ShareToData object for a single `url_with_title`, which is not
// associated to a WebState.
ShareToData* ShareToDataForURLWithTitle(URLWithTitle* url_with_title);

}  // namespace activity_services

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_SHARE_TO_DATA_BUILDER_H_
