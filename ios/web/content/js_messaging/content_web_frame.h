// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_WEB_FRAME_H_
#define IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_WEB_FRAME_H_

#import <map>
#import <string>

#import "base/cancelable_callback.h"
#import "base/memory/weak_ptr.h"
#import "base/values.h"
#import "ios/web/js_messaging/web_frame_internal.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/web_state_observer.h"
#import "url/gurl.h"

namespace content {
class RenderFrameHost;
}

namespace web {

class ContentWebState;

// ContentWebFrame is a WebFrame that wraps a content::RenderFrameHost.
class ContentWebFrame : public WebFrame,
                        public WebFrameInternal,
                        public WebStateObserver {
 public:
  ContentWebFrame(const std::string& web_frame_id,
                  content::RenderFrameHost* render_frame_id,
                  ContentWebState* content_web_state);

  ContentWebFrame(const ContentWebFrame&) = delete;
  ContentWebFrame& operator=(const ContentWebFrame&) = delete;

  ~ContentWebFrame() override;

  // WebFrame:
  WebFrameInternal* GetWebFrameInternal() override;
  std::string GetFrameId() const override;
  bool IsMainFrame() const override;
  GURL GetSecurityOrigin() const override;
  BrowserState* GetBrowserState() override;
  base::WeakPtr<WebFrame> AsWeakPtr() override;

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

  // WebFrameInternal:
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
  void WebStateDestroyed(WebState* web_state) override;

 private:
  // Detaches the receiver from the associated  WebState.
  void DetachFromWebState();

  // The web frame identifier which uniquely identifies this frame across the
  // application's lifetime.
  std::string web_frame_id_;

  // The web state corresponding to the WebContents for this frame.
  raw_ptr<ContentWebState> content_web_state_;

  // The RenderFrameHost corresponding to this frame.
  raw_ptr<content::RenderFrameHost> render_frame_host_;

  base::WeakPtrFactory<ContentWebFrame> weak_ptr_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_WEB_FRAME_H_
