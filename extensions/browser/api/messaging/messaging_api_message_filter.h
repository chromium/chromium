// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MESSAGING_MESSAGING_API_MESSAGE_FILTER_H_
#define EXTENSIONS_BROWSER_API_MESSAGING_MESSAGING_API_MESSAGE_FILTER_H_

#include "base/callback_list.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"

struct ExtensionMsg_ExternalConnectionInfo;
struct ExtensionMsg_TabTargetConnectionInfo;

namespace content {
class BrowserContext;
class RenderProcessHost;
}

namespace extensions {
struct Message;
struct PortContext;
struct PortId;

// This class filters out incoming messaging api-specific IPCs from the renderer
// process.
class MessagingAPIMessageFilter : public content::BrowserMessageFilter {
 public:
  MessagingAPIMessageFilter(int render_process_id,
                            content::BrowserContext* context);

  MessagingAPIMessageFilter(const MessagingAPIMessageFilter&) = delete;
  MessagingAPIMessageFilter& operator=(const MessagingAPIMessageFilter&) =
      delete;

  static void EnsureAssociatedFactoryBuilt();

 private:
  friend class base::DeleteHelper<MessagingAPIMessageFilter>;
  friend class content::BrowserThread;

  ~MessagingAPIMessageFilter() override;

  void Shutdown();

  // Returns the process that the IPC came from, or `nullptr` if the IPC should
  // be dropped (in case the IPC arrived racily after the process or its
  // BrowserContext already got destructed).
  content::RenderProcessHost* GetRenderProcessHost();

  // content::BrowserMessageFilter implementation:
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnOpenChannelToExtension(const PortContext& source_context,
                                const ExtensionMsg_ExternalConnectionInfo& info,
                                const std::string& channel_name,
                                const extensions::PortId& port_id);
  void OnOpenChannelToNativeApp(const PortContext& source_context,
                                const std::string& native_app_name,
                                const extensions::PortId& port_id);
  void OnOpenChannelToTab(const PortContext& source_context,
                          const ExtensionMsg_TabTargetConnectionInfo& info,
                          const std::string& channel_name,
                          const extensions::PortId& port_id);
  void OnOpenMessagePort(const PortContext& port_context,
                         const extensions::PortId& port_id);
  void OnCloseMessagePort(const PortContext& context,
                          const extensions::PortId& port_id,
                          bool force_close);
  void OnPostMessage(const extensions::PortId& port_id,
                     const extensions::Message& message);
  void OnResponsePending(const PortContext& port_context,
                         const PortId& port_id);

  const int render_process_id_;

  base::CallbackListSubscription shutdown_notifier_subscription_;

  // Only access from the UI thread.
  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MESSAGING_MESSAGING_API_MESSAGE_FILTER_H_
