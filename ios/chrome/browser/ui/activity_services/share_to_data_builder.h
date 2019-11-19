// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_TO_DATA_BUILDER_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_TO_DATA_BUILDER_H_

class GURL;

@class ShareToData;

namespace web {
class WebState;
}

namespace activity_services {

// Returns a ShareToData object using data from |web_state|. |share_url| is the
// URL to be shared with share extensions (excluding password managers). If
// |share_url| is empty, the visible URL associated with |web_state| will be
// used instead. |web_state| must not be nil. Function may return nil.
ShareToData* ShareToDataForWebState(web::WebState* web_state,
                                    const GURL& share_url);

}  // namespace activity_services

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_TO_DATA_BUILDER_H_
