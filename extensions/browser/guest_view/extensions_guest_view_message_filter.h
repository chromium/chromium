// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGE_FILTER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGE_FILTER_H_

#include <stdint.h>

#include <string>

#include "components/guest_view/browser/guest_view_message_filter.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
class BrowserContext;
}

namespace guest_view {
class GuestViewManager;
}

namespace extensions {
// This class filters out incoming extensions GuestView-specific IPC messages
// from the renderer process. It is created on the UI thread. Messages may be
// handled on the IO thread or the UI thread.
class ExtensionsGuestViewMessageFilter
    : public guest_view::GuestViewMessageFilter,
      public content::BrowserAssociatedInterface<mojom::GuestView> {
 public:
  ExtensionsGuestViewMessageFilter(int render_process_id,
                                   content::BrowserContext* context);
  ExtensionsGuestViewMessageFilter(const ExtensionsGuestViewMessageFilter&) =
      delete;
  ExtensionsGuestViewMessageFilter& operator=(
      const ExtensionsGuestViewMessageFilter&) = delete;

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<ExtensionsGuestViewMessageFilter>;

  ~ExtensionsGuestViewMessageFilter() override = default;

  // GuestViewMessageFilter implementation.
  guest_view::GuestViewManager* GetOrCreateGuestViewManager() override;

  // mojom::GuestView implementation.
  void ReadyToCreateMimeHandlerView(int32_t render_frame_id,
                                    bool success) override;
  void CanExecuteContentScript(
      int routing_id,
      const std::string& script_id,
      CanExecuteContentScriptCallback callback) override;

  static const uint32_t kFilteredMessageClasses[];
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGE_FILTER_H_
