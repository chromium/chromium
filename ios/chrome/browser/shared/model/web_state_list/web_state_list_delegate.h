// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_DELEGATE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_DELEGATE_H_

namespace web {
class WebState;
}

// A delegate interface that the WebStateList uses to perform work that it
// cannot do itself such as attaching tab helpers, ...
//
// See src/docs/ios/objects.md for more information.
class WebStateListDelegate {
 public:
  WebStateListDelegate() = default;

  WebStateListDelegate(const WebStateListDelegate&) = delete;
  WebStateListDelegate& operator=(const WebStateListDelegate&) = delete;

  virtual ~WebStateListDelegate() = default;

  // Notifies the delegate that the specified WebState will be added to the
  // WebStateList (via insertion/appending/replacing existing) and allows it
  // to do any preparation that it deems necessary.
  virtual void WillAddWebState(web::WebState* web_state) = 0;

  // Notifies the delegate that the specified WebState will become active
  // and allows it to do any preparation that it deems necessary.
  virtual void WillActivateWebState(web::WebState* web_state) = 0;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_DELEGATE_H_
