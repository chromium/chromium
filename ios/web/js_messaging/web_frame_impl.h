// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_WEB_FRAME_IMPL_H_
#define IOS_WEB_JS_MESSAGING_WEB_FRAME_IMPL_H_


#include <map>
#include <string>

#include "base/cancelable_callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#include "base/values.h"
#include "ios/web/js_messaging/web_frame_internal.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "url/gurl.h"

@class WKFrameInfo;

namespace web {

class JavaScriptContentWorld;

class WebFrameImpl final : public WebFrame,
                           public WebFrameInternal,
                           public web::WebStateObserver {
 public:
  // Creates a new WebFrame.
  WebFrameImpl(WKFrameInfo* frame_info,
               const std::string& frame_id,
               bool is_main_frame,
               GURL security_origin,
               web::WebState* web_state);

  WebFrameImpl(const WebFrameImpl&) = delete;
  WebFrameImpl& operator=(const WebFrameImpl&) = delete;

  ~WebFrameImpl() override;

  // The associated web state.
  WebState* GetWebState();

  // WebFrame:
  WebFrameInternal* GetWebFrameInternal() override;
  std::string GetFrameId() const override;
  bool IsMainFrame() const override;
  GURL GetSecurityOrigin() const override;
  BrowserState* GetBrowserState() override;

  bool CallJavaScriptFunction(const std::string& name,
                              const base::Value::List& parameters) override;
  bool CallJavaScriptFunction(
      const std::string& name,
      const base::Value::List& parameters,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout) override;

  bool ExecuteJavaScript(const std::u16string& script) override;
  bool ExecuteJavaScript(
      const std::u16string& script,
      base::OnceCallback<void(const base::Value*)> callback) override;
  bool ExecuteJavaScript(const std::u16string& script,
                         ExecuteJavaScriptCallbackWithError callback) override;
  base::WeakPtr<WebFrame> AsWeakPtr() override;

  // WebFrameContentWorldAPI:
  bool CallJavaScriptFunctionInContentWorld(
      const std::string& name,
      const base::Value::List& parameters,
      JavaScriptContentWorld* content_world) override;
  bool CallJavaScriptFunctionInContentWorld(
      const std::string& name,
      const base::Value::List& parameters,
      JavaScriptContentWorld* content_world,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout) override;
  bool ExecuteJavaScriptInContentWorld(
      const std::u16string& script,
      JavaScriptContentWorld* content_world,
      ExecuteJavaScriptCallbackWithError callback) override;

  // WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  // Calls the JavaScript function `name` in the frame context in the same
  // manner as the inherited CallJavaScriptFunction functions. `content_world`
  // is optional, but if specified, the function will be executed within that
  // world. If `reply_with_result` is true, the return value of executing the
  // function will be sent back to the receiver with `CompleteRequest()`.
  bool CallJavaScriptFunctionInContentWorld(
      const std::string& name,
      const base::Value::List& parameters,
      JavaScriptContentWorld* content_world,
      bool reply_with_result);

  // Detaches the receiver from the associated  WebState.
  void DetachFromWebState();

  // A structure to store the callbacks associated with the
  // `CallJavaScriptFunction` requests.
  typedef base::CancelableOnceCallback<void(void)> TimeoutCallback;
  struct RequestCallbacks {
    RequestCallbacks(base::OnceCallback<void(const base::Value*)> completion,
                     std::unique_ptr<TimeoutCallback>);
    ~RequestCallbacks();
    base::OnceCallback<void(const base::Value*)> completion;
    std::unique_ptr<TimeoutCallback> timeout_callback;
  };

  // Calls the JavaScript function `name` in the web state. If `content_world`
  // is specified, the function will be executed within `content_world`. If
  // `reply_with_result` is true, the return value of executing the function
  // will be sent back to the receiver.
  bool ExecuteJavaScriptFunction(JavaScriptContentWorld* content_world,
                                 const std::string& name,
                                 const base::Value::List& parameters,
                                 int message_id,
                                 bool reply_with_result);

  // Converts the given callback into a `ExecuteJavaScriptCallbackWithError`
  // callback. This function improves code sharing by being a bridge
  // between the various ExecuteJavaScript() functions.
  ExecuteJavaScriptCallbackWithError ExecuteJavaScriptCallbackAdapter(
      base::OnceCallback<void(const base::Value*)> callback);
  // Prints the information about the error that was generated from the
  // execution of the given arbitrary JavaScript string.
  void LogScriptWarning(NSString* script, NSError* error);

  // Runs the request associated with the message with id `message_id`. The
  // completion callback, if any, associated with `message_id` will be called
  // with `result`.
  void CompleteRequest(int message_id, const base::Value* result);
  // Calls the completion block of `request_callbacks` with `result` value and
  // removes the callbacks from `pending_requests`.
  void CompleteRequest(std::unique_ptr<RequestCallbacks> request_callbacks,
                       const base::Value* result);

  // Cancels the request associated with the message with id `message_id`. The
  // completion callback, if any, associated with `message_id` will be called
  // with a null result value. Note that the JavaScript will still run to
  // completion, but any future response will be ignored.
  void CancelRequest(int message_id);
  // Performs `CancelRequest` on all outstanding request callbacks in
  // `pending_requests_`.
  void CancelPendingRequests();

  // The JavaScript requests awating a reply.
  std::map<uint32_t, std::unique_ptr<struct RequestCallbacks>>
      pending_requests_;

  // The frame info instance associated with this web frame.
  WKFrameInfo* frame_info_;
  // The frame identifier which uniquely identifies this frame across the
  // application's lifetime.
  std::string frame_id_;
  // The message ID of the next JavaScript message to be sent.
  int next_message_id_ = 0;
  // Whether or not the receiver represents the main frame.
  bool is_main_frame_ = false;
  // The security origin associated with this frame.
  GURL security_origin_;
  // The associated web state.
  raw_ptr<web::WebState> web_state_ = nullptr;

  base::WeakPtrFactory<WebFrameImpl> weak_ptr_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_WEB_FRAME_IMPL_H_
