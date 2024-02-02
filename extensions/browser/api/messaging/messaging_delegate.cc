// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/messaging_delegate.h"

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "extensions/common/extension_id.h"

namespace extensions {

MessagingDelegate::PolicyPermission
MessagingDelegate::IsNativeMessagingHostAllowed(
    content::BrowserContext* browser_context,
    const std::string& native_host_name) {
  NOTIMPLEMENTED();
  return PolicyPermission::DISALLOW;
}

std::optional<base::Value::Dict> MessagingDelegate::MaybeGetTabInfo(
    content::WebContents* web_contents) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

content::WebContents* MessagingDelegate::GetWebContentsByTabId(
    content::BrowserContext* browser_context,
    int tab_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<MessagePort> MessagingDelegate::CreateReceiverForTab(
    base::WeakPtr<MessagePort::ChannelDelegate> channel_delegate,
    const ExtensionId& extension_id,
    const PortId& receiver_port_id,
    content::WebContents* receiver_contents,
    int receiver_frame_id,
    const std::string& receiver_document_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<MessagePort> MessagingDelegate::CreateReceiverForNativeApp(
    content::BrowserContext* browser_context,
    base::WeakPtr<MessagePort::ChannelDelegate> channel_delegate,
    content::RenderFrameHost* source,
    const ExtensionId& extension_id,
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
    base::OnceCallback<void(bool)> callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
}

}  // namespace extensions
