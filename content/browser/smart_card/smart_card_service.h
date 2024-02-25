// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_
#define CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
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
      public device::mojom::SmartCardContext {
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
               ConnectCallback callback) override;

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

  // Receives SmartCardContext calls from blink
  mojo::ReceiverSet<device::mojom::SmartCardContext> context_wrapper_receivers_;

  // Sends SmartCardContext calls to the platform's PC/SC stack.
  // Maps a wrapper context to its corresponding real context.
  std::map<mojo::ReceiverId, mojo::Remote<SmartCardContext>> context_remotes_;

  // Used to filter a reader name coming from an application, before
  // it can be shown to the user in a permission prompt.
  base::flat_set<std::string> valid_reader_names_;

  mojo::Remote<device::mojom::SmartCardContextFactory> context_factory_;
  base::WeakPtrFactory<SmartCardService> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_
