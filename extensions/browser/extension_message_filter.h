// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_MESSAGE_FILTER_H_
#define EXTENSIONS_BROWSER_EXTENSION_MESSAGE_FILTER_H_

#include "base/callback_list.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {
class BrowserContext;
}

namespace extensions {
struct Message;

// This class filters out incoming extension-specific IPC messages from the
// renderer process. It is created and destroyed on the UI thread and handles
// messages there.
class ExtensionMessageFilter : public content::BrowserMessageFilter {
 public:
  ExtensionMessageFilter(int render_process_id,
                         content::BrowserContext* context);

  ExtensionMessageFilter(const ExtensionMessageFilter&) = delete;
  ExtensionMessageFilter& operator=(const ExtensionMessageFilter&) = delete;

  int render_process_id() { return render_process_id_; }

  static void EnsureShutdownNotifierFactoryBuilt();

 private:
  friend class base::DeleteHelper<ExtensionMessageFilter>;
  friend class content::BrowserThread;

  ~ExtensionMessageFilter() override;

  void ShutdownOnUIThread();

  // content::BrowserMessageFilter implementation:
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Message handlers on the UI thread.
  void OnExtensionTransferBlobsAck(const std::vector<std::string>& blob_uuids);
  void OnExtensionWakeEventPage(int request_id,
                                const std::string& extension_id);

  // Responds to the ExtensionHostMsg_WakeEventPage message.
  void SendWakeEventPageResponse(int request_id, bool success);

  const int render_process_id_;

  base::CallbackListSubscription shutdown_notifier_subscription_;

  // Only access from the UI thread.
  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_MESSAGE_FILTER_H_
