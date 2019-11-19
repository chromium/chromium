// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_WEB_FRAME_IMPL_H_
#define IOS_WEB_JS_MESSAGING_WEB_FRAME_IMPL_H_

#include "ios/web/public/js_messaging/web_frame.h"

#include <map>
#include <string>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "crypto/symmetric_key.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "url/gurl.h"

namespace web {

class WebFrameImpl : public WebFrame, public web::WebStateObserver {
 public:
  // Creates a new WebFrame. |initial_message_id| will be used as the message ID
  // of the next message sent to the frame with the |CallJavaScriptFunction|
  // API.
  WebFrameImpl(const std::string& frame_id,
               bool is_main_frame,
               GURL security_origin,
               web::WebState* web_state);
  ~WebFrameImpl() override;

  // Sets the value to use for the next message ID.
  void SetNextMessageId(int message_id);
  // Sets the key to use for message encryption.
  void SetEncryptionKey(std::unique_ptr<crypto::SymmetricKey> frame_key);
  // The associated web state.
  WebState* GetWebState();

  // WebFrame implementation
  std::string GetFrameId() const override;
  bool IsMainFrame() const override;
  GURL GetSecurityOrigin() const override;
  bool CanCallJavaScriptFunction() const override;

  bool CallJavaScriptFunction(
      const std::string& name,
      const std::vector<base::Value>& parameters) override;
  bool CallJavaScriptFunction(
      const std::string& name,
      const std::vector<base::Value>& parameters,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout) override;

  // WebStateObserver implementation
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  // Calls the JavaScript function |name| in the frame context in the same
  // manner as the inherited CallJavaScriptFunction functions. If
  // |reply_with_result| is true, the return value of executing the function
  // will be sent back to the receiver and handled by |OnJavaScriptReply|.
  bool CallJavaScriptFunction(const std::string& name,
                              const std::vector<base::Value>& parameters,
                              bool reply_with_result);

  // Detaches the receiver from the associated  WebState.
  void DetachFromWebState();
  // Returns the script command name to use for this WebFrame.
  const std::string GetScriptCommandPrefix();
  // Encrypts |payload| and returns a JSON string of a dictionary containing
  // the encrypted metadata and its initialization vector. If encryption fails,
  // an empty string will be returned.
  const std::string EncryptPayload(base::DictionaryValue payload,
                                   const std::string& additiona_data);

  // A structure to store the callbacks associated with the
  // |CallJavaScriptFunction| requests.
  typedef base::CancelableOnceCallback<void(void)> TimeoutCallback;
  struct RequestCallbacks {
    RequestCallbacks(base::OnceCallback<void(const base::Value*)> completion,
                     std::unique_ptr<TimeoutCallback>);
    ~RequestCallbacks();
    base::OnceCallback<void(const base::Value*)> completion;
    std::unique_ptr<TimeoutCallback> timeout_callback;
  };

  // Calls the JavaScript function |name| in the web state (main frame). If
  // |reply_with_result| is true, the return value of executing the function
  // will be sent back to the receiver. This function is only used if the
  // receiver does not have an encryption key. The JavaScript function is called
  // directly and thus only works on the main frame. (Encryption is not required
  // to securely communicate with the main frame because evaluating JavaScript
  // on the WebState is already secure.)
  bool ExecuteJavaScriptFunction(const std::string& name,
                                 const std::vector<base::Value>& parameters,
                                 int message_id,
                                 bool reply_with_result);

  // Runs the request associated with the message with id |message_id|. The
  // completion callback, if any, associated with |message_id| will be called
  // with |result|.
  void CompleteRequest(int message_id, const base::Value* result);
  // Calls the completion block of |request_callbacks| with |result| value and
  // removes the callbacks from |pending_requests|.
  void CompleteRequest(std::unique_ptr<RequestCallbacks> request_callbacks,
                       const base::Value* result);

  // Cancels the request associated with the message with id |message_id|. The
  // completion callback, if any, associated with |message_id| will be called
  // with a null result value. Note that the JavaScript will still run to
  // completion, but any future response will be ignored.
  void CancelRequest(int message_id);
  // Performs |CancelRequest| on all outstanding request callbacks in
  // |pending_requests_|.
  void CancelPendingRequests();

  // Handles message from JavaScript with result of executing the function
  // specified in CallJavaScriptFunction.
  void OnJavaScriptReply(web::WebState* web_state,
                         const base::DictionaryValue& command,
                         const GURL& page_url,
                         bool interacting,
                         WebFrame* sender_frame);

  // The JavaScript requests awating a reply.
  std::map<uint32_t, std::unique_ptr<struct RequestCallbacks>>
      pending_requests_;

  // The frame identifier which uniquely identifies this frame across the
  // application's lifetime.
  std::string frame_id_;
  // The symmetric encryption key used to encrypt messages addressed to the
  // frame. Stored in a base64 encoded string.
  std::unique_ptr<crypto::SymmetricKey> frame_key_;
  // The message ID of the next JavaScript message to be sent.
  int next_message_id_ = 0;
  // Whether or not the receiver represents the main frame.
  bool is_main_frame_ = false;
  // The security origin associated with this frame.
  GURL security_origin_;
  // The associated web state.
  web::WebState* web_state_ = nullptr;
  // Subscription for JS message.
  std::unique_ptr<web::WebState::ScriptCommandSubscription> subscription_;

  base::WeakPtrFactory<WebFrameImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameImpl);
};

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_WEB_FRAME_IMPL_H_
