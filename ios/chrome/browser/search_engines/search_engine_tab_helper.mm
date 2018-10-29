// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/search_engine_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(SearchEngineTabHelper);

SearchEngineTabHelper::~SearchEngineTabHelper() {}

SearchEngineTabHelper::SearchEngineTabHelper(web::WebState* web_state)
    : web_state_(web_state), weak_ptr_factory_(this) {
  web_state->AddObserver(this);
}

void SearchEngineTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  web_state_ = nullptr;
}
