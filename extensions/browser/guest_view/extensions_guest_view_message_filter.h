// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGE_FILTER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGE_FILTER_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/guest_view/browser/guest_view_message_filter.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace gfx {
class Size;
}

namespace guest_view {
class GuestViewManager;
}

namespace extensions {
// This class filters out incoming extensions GuestView-specific IPC messages
// from thw renderer process. It is created on the UI thread. Messages may be
// handled on the IO thread or the UI thread.
class ExtensionsGuestViewMessageFilter
    : public guest_view::GuestViewMessageFilter,
      public content::BrowserAssociatedInterface<mojom::GuestView>,
      public mojom::GuestView {
 public:
  ExtensionsGuestViewMessageFilter(int render_process_id,
                                   content::BrowserContext* context);

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<ExtensionsGuestViewMessageFilter>;

  ~ExtensionsGuestViewMessageFilter() override;

  // GuestViewMessageFilter implementation.
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  guest_view::GuestViewManager* GetOrCreateGuestViewManager() override;

  // Message handlers on the UI thread.
  void OnCanExecuteContentScript(int render_view_id,
                                 int script_id,
                                 bool* allowed);
  void OnCreateMimeHandlerViewGuest(int render_frame_id,
                                    const std::string& view_id,
                                    int element_instance_id,
                                    const gfx::Size& element_size);
  void OnResizeGuest(int render_frame_id,
                     int element_instance_id,
                     const gfx::Size& new_size);

  // mojom::GuestView implementation.
  void CreateEmbeddedMimeHandlerViewGuest(
      int32_t render_frame_id,
      int32_t tab_id,
      const GURL& original_url,
      int32_t element_instance_id,
      const gfx::Size& element_size,
      content::mojom::TransferrableURLLoaderPtr transferrable_url_loader)
      override;
  void CreateMimeHandlerViewGuest(
      int32_t render_frame_id,
      const std::string& view_id,
      int32_t element_instance_id,
      const gfx::Size& element_size,
      mojo::PendingRemote<mime_handler::BeforeUnloadControl>
          before_unload_control) override;
  void ReadyToCreateMimeHandlerView(int32_t render_frame_id,
                                    bool success) override;

  void CreateMimeHandlerViewGuestOnUIThread(
      int32_t render_frame_id,
      const std::string& view_id,
      int32_t element_instance_id,
      const gfx::Size& element_size,
      mojo::PendingRemote<mime_handler::BeforeUnloadControl>
          before_unload_control,
      bool is_full_page_plugin);

  // Runs on UI thread.
  void MimeHandlerViewGuestCreatedCallback(
      int element_instance_id,
      int embedder_render_process_id,
      int embedder_render_frame_id,
      const gfx::Size& element_size,
      mojo::PendingRemote<mime_handler::BeforeUnloadControl>
          before_unload_control,
      bool is_full_page_plugin,
      content::WebContents* web_contents);

  static const uint32_t kFilteredMessageClasses[];

  DISALLOW_COPY_AND_ASSIGN(ExtensionsGuestViewMessageFilter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MESSAGE_FILTER_H_
