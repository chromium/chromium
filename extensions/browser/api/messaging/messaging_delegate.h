// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MESSAGING_MESSAGING_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_MESSAGING_MESSAGING_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/api/messaging/message_port.h"
#include "extensions/common/extension_id.h"

class GURL;

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
struct PortId;

// Helper class for embedder-specific extension messaging API support for tabs
// and native apps.
class MessagingDelegate {
 public:
  enum class PolicyPermission {
    DISALLOW,           // The host is not allowed.
    ALLOW_SYSTEM_ONLY,  // Allowed only when installed on system level.
    ALLOW_ALL,          // Allowed when installed on system or user level.
  };

  virtual ~MessagingDelegate() = default;

  // Checks whether native messaging is allowed for the given host.
  virtual PolicyPermission IsNativeMessagingHostAllowed(
      content::BrowserContext* browser_context,
      const std::string& native_host_name);

  // If web_contents is a tab, returns a dictionary representing its tab.
  // Otherwise returns nullptr.
  virtual std::optional<base::Value::Dict> MaybeGetTabInfo(
      content::WebContents* web_contents);

  // Returns the WebContents for the given tab ID, if found.
  virtual content::WebContents* GetWebContentsByTabId(
      content::BrowserContext* browser_context,
      int tab_id);

  // Creates a MessagePort for the tab with the given ID and populates
  // |receiver_browser_context| with the tab's BrowserContext. Returns nullptr
  // if the tab is not available.
  virtual std::unique_ptr<MessagePort> CreateReceiverForTab(
      base::WeakPtr<MessagePort::ChannelDelegate> channel_delegate,
      const ExtensionId& extension_id,
      const PortId& receiver_port_id,
      content::WebContents* receiver_contents,
      int receiver_frame_id,
      const std::string& receiver_document_id);

  // Creates a MessagePort for a native app. If the port cannot be created,
  // returns nullptr and may populate |error_out|.
  virtual std::unique_ptr<MessagePort> CreateReceiverForNativeApp(
      content::BrowserContext* browser_context,
      base::WeakPtr<MessagePort::ChannelDelegate> channel_delegate,
      content::RenderFrameHost* source,
      const ExtensionId& extension_id,
      const PortId& receiver_port_id,
      const std::string& native_app_name,
      bool allow_user_level,
      std::string* error_out);

  // Runs |callback| with true if |url| is allowed to connect to |extension|
  // from incognito mode, false otherwise. If the URL's origin has not been
  // granted/denied access yet, the user may be prompted before the callback is
  // run with their response.
  virtual void QueryIncognitoConnectability(
      content::BrowserContext* context,
      const Extension* extension,
      content::WebContents* web_contents,
      const GURL& url,
      base::OnceCallback<void(bool)> callback);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MESSAGING_MESSAGING_DELEGATE_H_
