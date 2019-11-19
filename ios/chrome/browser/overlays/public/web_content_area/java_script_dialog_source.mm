// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_source.h"

#include "base/logging.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

JavaScriptDialogSource::JavaScriptDialogSource(web::WebState* web_state,
                                               const GURL& url,
                                               bool is_main_frame)
    : web_state_(web_state), url_(url), is_main_frame_(is_main_frame) {
  DCHECK(url_.is_valid());
  if (web_state_)
    web_state_->AddObserver(this);
}

JavaScriptDialogSource::JavaScriptDialogSource(
    const JavaScriptDialogSource& source)
    : JavaScriptDialogSource(source.web_state_,
                             source.url_,
                             source.is_main_frame_) {}

JavaScriptDialogSource::~JavaScriptDialogSource() {
  if (web_state_)
    web_state_->RemoveObserver(this);
}

void JavaScriptDialogSource::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}
