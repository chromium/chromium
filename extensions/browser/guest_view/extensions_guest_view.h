// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_H_

#include <stdint.h>

#include <string>

#include "components/guest_view/browser/guest_view_message_handler.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace extensions {

class ExtensionsGuestView : public guest_view::GuestViewMessageHandler,
                            public mojom::GuestView {
 public:
  ExtensionsGuestView(const ExtensionsGuestView&) = delete;
  ExtensionsGuestView& operator=(const ExtensionsGuestView&) = delete;
  ~ExtensionsGuestView() override;

  // GuestView messages are split between components/ and extensions/.
  // This class implements the interfaces of both layers.
  static void CreateForComponents(
      int render_process_id,
      mojo::PendingAssociatedReceiver<guest_view::mojom::GuestViewHost>
          receiver);
  static void CreateForExtensions(
      int render_process_id,
      mojo::PendingAssociatedReceiver<extensions::mojom::GuestView> receiver);

 private:
  explicit ExtensionsGuestView(int render_process_id);

  // guest_view::GuestViewMessageHandler:
  std::unique_ptr<guest_view::GuestViewManagerDelegate>
  CreateGuestViewManagerDelegate(
      content::BrowserContext* context) const override;

  // mojom::GuestView:
  void ReadyToCreateMimeHandlerView(int32_t render_frame_id,
                                    bool success) override;
  void CanExecuteContentScript(
      int routing_id,
      const std::string& script_id,
      CanExecuteContentScriptCallback callback) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_H_
