// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/js_messaging/web_frames_manager_impl.h"

#include "base/base64.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "crypto/symmetric_key.h"
#import "ios/web/js_messaging/crw_wk_script_message_router.h"
#include "ios/web/js_messaging/web_frame_impl.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_view/wk_security_origin_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Message command sent when a frame becomes available.
NSString* const kFrameBecameAvailableMessageName = @"FrameBecameAvailable";
// Message command sent when a frame is unloading.
NSString* const kFrameBecameUnavailableMessageName = @"FrameBecameUnavailable";
}  // namespace

namespace web {

WebFramesManagerImpl::WebFramesManagerImpl(WebFramesManagerDelegate& delegate)
    : delegate_(delegate), weak_factory_(this) {}

WebFramesManagerImpl::~WebFramesManagerImpl() {
  RemoveAllWebFrames();
}

void WebFramesManagerImpl::RemoveAllWebFrames() {
  while (web_frames_.size()) {
    RemoveFrameWithId(web_frames_.begin()->first);
  }
}

void WebFramesManagerImpl::RegisterExistingFrames() {
  delegate_.GetWebState()->ExecuteJavaScript(
      base::UTF8ToUTF16("__gCrWeb.message.getExistingFrames();"));
}

void WebFramesManagerImpl::OnWebViewUpdated(
    WKWebView* old_web_view,
    WKWebView* new_web_view,
    CRWWKScriptMessageRouter* message_router) {
  DCHECK(old_web_view != new_web_view);
  if (old_web_view) {
    RemoveAllWebFrames();
    // TODO(crbug.com/956516): ScriptMessageHandlers should all be removed
    // manually using |removeScriptMessageHandlerForName|, however this is not
    // possible because of the cases where the WKWebViewConfiguration is purged,
    // in these cases the message router is deleted and it will not have
    // message handlers for the web view.
    [message_router removeAllScriptMessageHandlersForWebView:old_web_view];
  }

  // |this| is captured inside callbacks for JS messages, so the owner of
  // WebFramesManagerImpl must call OnWebViewUpdated(last_web_view, nil) when
  // being destroyed, so that WebFramesManagerImpl can unregister callbacks in
  // time. This guarantees that when callbacks are invoked, |this| is always
  // valid.
  if (new_web_view) {
    // TODO(crbug.com/991950): Clean up lifecycles of WebStateImpl and
    // CRWWebController, ensure that callbacks will always be unregistered
    // successfully during destruction. Remove WeakPtr here and use plain "this"
    // instead.
    base::WeakPtr<WebFramesManagerImpl> weak_ptr = weak_factory_.GetWeakPtr();

    [message_router
        setScriptMessageHandler:^(WKScriptMessage* message) {
          if (weak_ptr) {
            weak_ptr->OnFrameBecameAvailable(message);
          }
        }
                           name:kFrameBecameAvailableMessageName
                        webView:new_web_view];
    [message_router
        setScriptMessageHandler:^(WKScriptMessage* message) {
          DCHECK(!delegate_.GetWebState()->IsBeingDestroyed());
          if (weak_ptr) {
            weak_ptr->OnFrameBecameUnavailable(message);
          }
        }
                           name:kFrameBecameUnavailableMessageName
                        webView:new_web_view];
  }
}

#pragma mark - WebFramesManager

std::set<WebFrame*> WebFramesManagerImpl::GetAllWebFrames() {
  std::set<WebFrame*> frames;
  for (const auto& it : web_frames_) {
    frames.insert(it.second.get());
  }
  return frames;
}

WebFrame* WebFramesManagerImpl::GetMainWebFrame() {
  return main_web_frame_;
}

WebFrame* WebFramesManagerImpl::GetFrameWithId(const std::string& frame_id) {
  DCHECK(!frame_id.empty());
  auto web_frames_it = web_frames_.find(frame_id);
  return web_frames_it == web_frames_.end() ? nullptr
                                            : web_frames_it->second.get();
}

#pragma mark - Private

void WebFramesManagerImpl::AddFrame(std::unique_ptr<WebFrame> frame) {
  DCHECK(frame);
  DCHECK(!frame->GetFrameId().empty());
  if (frame->IsMainFrame()) {
    DCHECK(!main_web_frame_);
    main_web_frame_ = frame.get();
  }
  DCHECK(web_frames_.count(frame->GetFrameId()) == 0);
  std::string frame_id = frame->GetFrameId();
  web_frames_[frame_id] = std::move(frame);

  delegate_.OnWebFrameAvailable(GetFrameWithId(frame_id));
}

void WebFramesManagerImpl::RemoveFrameWithId(const std::string& frame_id) {
  DCHECK(!frame_id.empty());
  // If the removed frame is a main frame, it should be the current one.
  DCHECK(web_frames_.count(frame_id) == 0 ||
         !web_frames_[frame_id]->IsMainFrame() ||
         main_web_frame_ == web_frames_[frame_id].get());
  if (web_frames_.count(frame_id) == 0) {
    return;
  }
  delegate_.OnWebFrameUnavailable(web_frames_[frame_id].get());
  if (main_web_frame_ && main_web_frame_->GetFrameId() == frame_id) {
    main_web_frame_ = nullptr;
  }
  // The web::WebFrame destructor can call some callbacks that will try to
  // access the frame via GetFrameWithId. This can lead to a reentrancy issue
  // on |web_frames_|.
  // To avoid this issue, keep the frame alive during the map operation and
  // destroy it after.
  auto keep_frame_alive = std::move(web_frames_[frame_id]);
  web_frames_.erase(frame_id);
}

void WebFramesManagerImpl::OnFrameBecameAvailable(WKScriptMessage* message) {
  DCHECK(!delegate_.GetWebState()->IsBeingDestroyed());
  // Validate all expected message components because any frame could falsify
  // this message.
  if (![message.body isKindOfClass:[NSDictionary class]] ||
      ![message.body[@"crwFrameId"] isKindOfClass:[NSString class]]) {
    return;
  }

  std::string frame_id = base::SysNSStringToUTF8(message.body[@"crwFrameId"]);
  if (!GetFrameWithId(frame_id)) {
    GURL message_frame_origin =
        web::GURLOriginWithWKSecurityOrigin(message.frameInfo.securityOrigin);

    std::unique_ptr<crypto::SymmetricKey> frame_key;
    if ([message.body[@"crwFrameKey"] isKindOfClass:[NSString class]] &&
        [message.body[@"crwFrameKey"] length] > 0) {
      std::string decoded_frame_key_string;
      std::string encoded_frame_key_string =
          base::SysNSStringToUTF8(message.body[@"crwFrameKey"]);
      base::Base64Decode(encoded_frame_key_string, &decoded_frame_key_string);
      frame_key = crypto::SymmetricKey::Import(
          crypto::SymmetricKey::Algorithm::AES, decoded_frame_key_string);
    }

    auto new_frame = std::make_unique<web::WebFrameImpl>(
        frame_id, message.frameInfo.mainFrame, message_frame_origin,
        delegate_.GetWebState());
    if (frame_key) {
      new_frame->SetEncryptionKey(std::move(frame_key));
    }

    NSNumber* last_sent_message_id =
        message.body[@"crwFrameLastReceivedMessageId"];
    if ([last_sent_message_id isKindOfClass:[NSNumber class]]) {
      int next_message_id = std::max(0, last_sent_message_id.intValue + 1);
      new_frame->SetNextMessageId(next_message_id);
    }

    AddFrame(std::move(new_frame));
  }
}

void WebFramesManagerImpl::OnFrameBecameUnavailable(WKScriptMessage* message) {
  DCHECK(!delegate_.GetWebState()->IsBeingDestroyed());
  if (![message.body isKindOfClass:[NSString class]]) {
    // WebController is being destroyed or message is invalid.
    return;
  }
  std::string frame_id = base::SysNSStringToUTF8(message.body);
  RemoveFrameWithId(frame_id);
}

}  // namespace
