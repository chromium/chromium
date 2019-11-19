// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/messaging_delegate.h"

#include "base/callback.h"
#include "base/logging.h"

namespace extensions {

MessagingDelegate::PolicyPermission
MessagingDelegate::IsNativeMessagingHostAllowed(
    content::BrowserContext* browser_context,
    const std::string& native_host_name) {
  NOTIMPLEMENTED();
  return PolicyPermission::DISALLOW;
}

std::unique_ptr<base::DictionaryValue> MessagingDelegate::MaybeGetTabInfo(
    content::WebContents* web_contents) {
  NOTIMPLEMENTED();
  return nullptr;
}

content::WebContents* MessagingDelegate::GetWebContentsByTabId(
    content::BrowserContext* browser_context,
    int tab_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<MessagePort> MessagingDelegate::CreateReceiverForTab(
    base::WeakPtr<MessagePort::ChannelDelegate> channel_delegate,
    const std::string& extension_id,
    const PortId& receiver_port_id,
    content::WebContents* receiver_contents,
    int receiver_frame_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<MessagePort> MessagingDelegate::CreateReceiverForNativeApp(
    content::BrowserContext* browser_context,
    base::WeakPtr<MessagePort::ChannelDelegate> channel_delegate,
    content::RenderFrameHost* source,
    const std::string& extension_id,
    const PortId& receiver_port_id,
    const std::string& native_app_name,
    bool allow_user_level,
    std::string* error_out) {
  NOTIMPLEMENTED();
  return nullptr;
}

void MessagingDelegate::QueryIncognitoConnectability(
    content::BrowserContext* context,
    const Extension* target_extension,
    content::WebContents* source_contents,
    const GURL& source_url,
    const base::Callback<void(bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(false);
}

}  // namespace extensions
