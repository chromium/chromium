// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAME_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAME_H_

#include <string>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace web {

class WebFrame : public base::SupportsUserData {
 public:
  // The frame identifier which uniquely identifies this frame across the
  // application's lifetime.
  virtual std::string GetFrameId() const = 0;
  // Whether or not the receiver represents the main frame of the webpage.
  virtual bool IsMainFrame() const = 0;
  // The security origin associated with this frame.
  virtual GURL GetSecurityOrigin() const = 0;
  // Whether or not the receiver represents a frame which supports calling
  // JavaScript functions using |CallJavaScriptFunction()|.
  virtual bool CanCallJavaScriptFunction() const = 0;

  // Calls the JavaScript function |name| in the frame context. For example, to
  // call __gCrWeb.formHandlers.trackFormMutations(delay), pass
  // 'form.trackFormMutations' as |name| and the value for the delay parameter
  // to |parameters|. |name| must point to a function in the __gCrWeb object.
  // |parameters| is a vector of values that will be passed to the function.
  // This method returns immediately without waiting for the JavaScript
  // execution. Calling the function is best effort and it is possible the
  // webpage DOM could change in a way which prevents the function from
  // executing.
  // Returns true if function call was requested, false otherwise. Function call
  // may still fail even if this function returns true. Always returns false if
  // |CanCallJavaScriptFunction| is false.
  virtual bool CallJavaScriptFunction(
      const std::string& name,
      const std::vector<base::Value>& parameters) = 0;

  // Calls the JavaScript function in the same condition as
  // CallJavaScriptFunction(std::string, const std::vector<base::Value>&).
  // |callback| will be called with the value returned by the method.
  // If |timeout| is reached, callback is called with the nullptr parameter
  // and no result received later will be sent.
  // Returns true if function call was requested, false otherwise. Function call
  // may still fail even if this function returns true. Always returns false if
  // |CanCallJavaScriptFunction| is false.
  virtual bool CallJavaScriptFunction(
      const std::string& name,
      const std::vector<base::Value>& parameters,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout) = 0;

  ~WebFrame() override {}

 protected:
  WebFrame() {}

  DISALLOW_COPY_AND_ASSIGN(WebFrame);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAME_H_
