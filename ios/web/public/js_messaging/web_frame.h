// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAME_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAME_H_

#import <Foundation/Foundation.h>

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "base/values.h"
#include "url/gurl.h"

namespace web {

class BrowserState;
class WebFrameInternal;

// Default timeout in milliseconds for `CallJavaScriptFunction`.
extern const double kJavaScriptFunctionCallDefaultTimeout;

using ExecuteJavaScriptCallbackWithError =
    base::OnceCallback<void(const base::Value*, NSError* error)>;

class WebFrame : public base::SupportsUserData {
 public:
  // The frame identifier which uniquely identifies this frame across the
  // application's lifetime.
  virtual std::string GetFrameId() const = 0;
  // Whether or not the receiver represents the main frame of the webpage.
  // TODO(crbug.com/40216361): Rename IsMainFrame to IsAnyMainFrame
  virtual bool IsMainFrame() const = 0;
  // The security origin associated with this frame.
  virtual GURL GetSecurityOrigin() const = 0;

  // Returns the BrowserState associated with this WebFrame.
  virtual BrowserState* GetBrowserState() = 0;

  // Calls the JavaScript function `name` in the frame context. For example, to
  // call __gCrWeb.formHandlers.trackFormMutations(delay), pass
  // 'form.trackFormMutations' as `name` and the value for the delay parameter
  // to `parameters`. `name` must point to a function in the __gCrWeb object.
  // `parameters` is a vector of values that will be passed to the function.
  // This method returns immediately without waiting for the JavaScript
  // execution. Calling the function is best effort and it is possible the
  // webpage DOM could change in a way which prevents the function from
  // executing.
  // Returns true if function call was requested, false otherwise. Function call
  // may still fail even if this function returns true. Always returns false if
  // `CanCallJavaScriptFunction` is false.
  virtual bool CallJavaScriptFunction(const std::string& name,
                                      const base::Value::List& parameters) = 0;

  // Calls the JavaScript function in the same condition as
  // CallJavaScriptFunction(std::string, const base::Value::List&).
  // `callback` will be called with the value returned by the method.
  // If `timeout` is reached, callback is called with the nullptr parameter
  // and no result received later will be sent.
  // Returns true if function call was requested, false otherwise. Function call
  // may still fail even if this function returns true. Always returns false if
  // `CanCallJavaScriptFunction` is false.
  virtual bool CallJavaScriptFunction(
      const std::string& name,
      const base::Value::List& parameters,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout) = 0;

  // Executes the given `script` and returns whether the script was run.
  virtual bool ExecuteJavaScript(const std::u16string& script) = 0;

  // Executes the given `script` and returns whether the script was run.
  // If the script is successfully executed, `callback` is called with
  // the result.
  virtual bool ExecuteJavaScript(
      const std::u16string& script,
      base::OnceCallback<void(const base::Value*)> callback) = 0;

  // Executes the given `script` and returns whether the script was run.
  // If the script is successfully executed, `callback` is called with
  // the result. Otherwise, `callback` is called with the bool. The
  // bool parameter in the `callback` is used to signal that an error
  // during the execution of the `script` occurred.
  virtual bool ExecuteJavaScript(
      const std::u16string& script,
      ExecuteJavaScriptCallbackWithError callback) = 0;

  // Returns the WebFrameInternal instance for this object.
  virtual WebFrameInternal* GetWebFrameInternal() = 0;

  // Gets a weak pointer to the instance.
  virtual base::WeakPtr<WebFrame> AsWeakPtr() = 0;

  WebFrame(const WebFrame&) = delete;
  WebFrame& operator=(const WebFrame&) = delete;

  ~WebFrame() override {}

 protected:
  WebFrame() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAME_H_
