// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_OPENER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_OPENER_H_

#import "base/memory/raw_ptr.h"

namespace web {
class WebState;
}

// Represents the opener of a WebState.
struct WebStateOpener {
  // WebState responsible for the creation of the new WebState. May be null if
  // the WebState has no opener.
  raw_ptr<web::WebState> opener;

  // Recorded value of the `opener` last committed navigation index when the
  // WebState was open. Value is undefined if `opener` is null.
  int navigation_index;

  // Creates WebStateOpener with a null `opener`.
  WebStateOpener();

  // Creates WebStateOpener initialising the members from `opener` (the
  // `navigation_index` will be initialised from `opener`'s navigation
  // manager if `opener` is not null).
  explicit WebStateOpener(web::WebState* opener);

  // Creates WebStateOpener initialising the members from the parameters.
  WebStateOpener(web::WebState* opener, int navigation_index);
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_OPENER_H_
