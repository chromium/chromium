// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_
#define CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_

#include <map>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/smart_card_delegate.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/smart_card.mojom.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom.h"

namespace content {

class RenderFrameHost;

// SmarCardService provides an implementation of the SmartCardService mojom
// interface. This interface is used by Blink to implement the Web Smart Card
// API.
class CONTENT_EXPORT SmartCardService
    : public DocumentService<blink::mojom::SmartCardService>,
      public device::mojom::SmartCardContext,
      public device::mojom::SmartCardConnectionWatcher,
      public SmartCardDelegate::PermissionObserver {
 public:
  explicit SmartCardService(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::SmartCardService> receiver,
      mojo::PendingRemote<device::mojom::SmartCardContextFactory>
          context_factory);
  ~SmartCardService() override;

  // Use this when creating from a document.
  static void Create(RenderFrameHost*,
                     mojo::PendingReceiver<blink::mojom::SmartCardService>);

  // blink::mojom::SmartCardService overrides:
  void CreateContext(CreateContextCallback callback) override;

  // device::mojom::SmartCardContext overrides:
  void ListReaders(ListReadersCallback callback) override;
  void GetStatusChange(
      base::TimeDelta timeout,
      std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states,
      GetStatusChangeCallback callback) override;
  void Cancel(CancelCallback callback) override;
  void Connect(const std::string& reader,
               device::mojom::SmartCardShareMode share_mode,
               device::mojom::SmartCardProtocolsPtr preferred_protocols,
               mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher>
                   connection_watcher,
               ConnectCallback callback) override;
  void NotifyConnectionUsed() override;

  // SmartCardDelegate::PermissionObserver overrides:
  void OnPermissionRevoked(const url::Origin& origin) override;

 private:
  void OnContextCreated(CreateContextCallback callback,
                        ::device::mojom::SmartCardCreateContextResultPtr);
  void OnReaderPermissionResult(
      mojo::ReceiverId context_wrapper_id,
      const std::string& reader,
      device::mojom::SmartCardShareMode share_mode,
      device::mojom::SmartCardProtocolsPtr preferred_protocols,
      ConnectCallback callback,
      bool granted);
  void OnMojoWrapperContextDisconnected();

  void OnListReadersResult(ListReadersCallback callback,
                           device::mojom::SmartCardListReadersResultPtr result);

  mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher>
  GetNewConnectionWatcher(const std::string& reader);

  void OnMojoWatcherPipeClosed();

  // Sends SmartCardContext calls to the platform's PC/SC stack.
  // Maps a wrapper context to its corresponding real context.
  std::map<mojo::ReceiverId, mojo::Remote<SmartCardContext>> context_remotes_;

  // Receives SmartCardContext calls from blink
  mojo::ReceiverSet<device::mojom::SmartCardContext> context_wrapper_receivers_;

  // Receives notifications about smart card reader usage from the
  // platform-specific implementation.
  mojo::ReceiverSet<device::mojom::SmartCardConnectionWatcher>
      connection_watcher_receivers_;

  // On grant expiry, this allows us to kill the unwanted connections using the
  // watcher's pipe.
  std::map<std::string, std::set<mojo::ReceiverId>>
      connection_watchers_per_reader_;
  std::map<mojo::ReceiverId, std::string> reader_names_per_watcher_;

  // Used to filter a reader name coming from an application, before
  // it can be shown to the user in a permission prompt.
  base::flat_set<std::string> valid_reader_names_;

  mojo::Remote<device::mojom::SmartCardContextFactory> context_factory_;
  base::WeakPtrFactory<SmartCardService> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_
