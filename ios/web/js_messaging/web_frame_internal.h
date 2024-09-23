// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_WEB_FRAME_INTERNAL_H_
#define IOS_WEB_JS_MESSAGING_WEB_FRAME_INTERNAL_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/values.h"
#include "ios/web/public/js_messaging/web_frame.h"

namespace web {

class JavaScriptContentWorld;

class WebFrameInternal {
 public:
  // Calls the JavaScript function `name` in the frame context in the same
  // manner as the inherited CallJavaScriptFunction functions. `content_world`
  // is optional, but if specified, the function will be executed within that
  // world.
  virtual bool CallJavaScriptFunctionInContentWorld(
      const std::string& name,
      const base::Value::List& parameters,
      JavaScriptContentWorld* content_world) = 0;

  // Calls the JavaScript function in the same condition as
  // `CallJavaScriptFunctionInContentWorld` above. In addition, `callback` will
  // be called with the value returned by the JavaScript execution if it
  // completes before `timeout` is reached. If `timeout` is reached, `callback`
  // is called with a null value.
  // Returns true if function call was requested, false otherwise. Function call
  // may still fail even if this function returns true. Always returns false if
  // `CanCallJavaScriptFunction` is false.
  virtual bool CallJavaScriptFunctionInContentWorld(
      const std::string& name,
      const base::Value::List& parameters,
      JavaScriptContentWorld* content_world,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout) = 0;

  // Use of this function is DISCOURAGED. Prefer the
  // `CallJavaScriptFunctionInContentWorld` family of functions instead to keep
  // the API clear and well defined.
  // Executes `script` in `content_world`.
  // See WebFrame::ExecuteJavaScript for details on `callback`.
  virtual bool ExecuteJavaScriptInContentWorld(
      const std::u16string& script,
      JavaScriptContentWorld* content_world,
      ExecuteJavaScriptCallbackWithError callback) = 0;
};

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_WEB_FRAME_INTERNAL_H_
