// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAME_USER_DATA_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAME_USER_DATA_H_

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "ios/web/public/js_messaging/web_frame.h"

namespace web {

// A base class for classes attached to, and scoped to, the lifetime of a
// WebFrame. The lifetime of a WebFrame is far shorter than the ligetime of
// a WebState. A WebState lives as long as the tab lives. Multiple WebFrames
// are created on each navigation. If the object is heavy, consider using a
// WebStateUserData, and keep the WebFrame context only as WebFrameUserData.
// For example:
//
// --- in foo.h ---
// class Foo : public web::WebFrameUserData<Foo> {
//  public:
//   ~Foo() override;
//   // ... more public stuff here ...
//  private:
//   explicit Foo(web::WebFrame* web_frame);
//   friend class web::WebFrameUserData<Foo>;
//   // ... more private stuff here ...
// }
//
template <typename T>
class WebFrameUserData : public base::SupportsUserData::Data {
 public:
  // Creates an object of type T, and attaches it to the specified WebFrame.
  // If an instance is already attached, does nothing.
  static void CreateForWebFrame(WebFrame* web_frame) {
    DCHECK(web_frame);
    if (!FromWebFrame(web_frame))
      web_frame->SetUserData(UserDataKey(), base::WrapUnique(new T(web_frame)));
  }

  // Retrieves the instance of type T that was attached to the specified
  // WebFrame (via CreateForWebFrame above) and returns it. If no instance
  // of the type was attached, returns null.
  static T* FromWebFrame(WebFrame* web_frame) {
    return static_cast<T*>(web_frame->GetUserData(UserDataKey()));
  }
  static const T* FromWebFrame(const WebFrame* web_frame) {
    return static_cast<const T*>(web_frame->GetUserData(UserDataKey()));
  }
  // Removes the instance attached to the specified WebFrame.
  static void RemoveFromWebFrame(WebFrame* web_frame) {
    web_frame->RemoveUserData(UserDataKey());
  }

 protected:
  static inline const void* UserDataKey() {
    static const int kId = 0;
    return &kId;
  }
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAME_USER_DATA_H_
