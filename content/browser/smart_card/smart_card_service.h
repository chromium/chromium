// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_
#define CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_

#include "base/containers/queue.h"
#include "base/memory/raw_ref.h"
#include "content/browser/smart_card/smart_card_reader_tracker.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/smart_card.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom.h"

namespace content {

class RenderFrameHostImpl;

// SmarCardService provides an implementation of the SmartCardService mojom
// interface. This interface is used by Blink to implement the Web Smart Card
// API.
class CONTENT_EXPORT SmartCardService
    : public blink::mojom::SmartCardService,
      public SmartCardReaderTracker::Observer {
 public:
  SmartCardService(mojo::PendingRemote<device::mojom::SmartCardContextFactory>
                       context_factory,
                   bool supports_reader_added_removed_notifications,
                   SmartCardReaderTracker& reader_tracker);
  ~SmartCardService() override;

  // Use this when creating from a document.
  static void Create(RenderFrameHostImpl*,
                     mojo::PendingReceiver<blink::mojom::SmartCardService>);

  // blink::mojom::SmartCardService overrides:
  void GetReadersAndStartTracking(
      GetReadersAndStartTrackingCallback callback) override;
  void RegisterClient(mojo::PendingAssociatedRemote<
                          blink::mojom::SmartCardServiceClient> client,
                      RegisterClientCallback callback) override;
  void Connect(const std::string& reader,
               device::mojom::SmartCardShareMode share_mode,
               device::mojom::SmartCardProtocolsPtr preferred_protocols,
               ConnectCallback callback) override;

  // SmartCardReaderTracker::Observer overrides:
  void OnReaderAdded(
      const blink::mojom::SmartCardReaderInfo& reader_info) override;
  void OnReaderRemoved(
      const blink::mojom::SmartCardReaderInfo& reader_info) override;
  void OnReaderChanged(
      const blink::mojom::SmartCardReaderInfo& reader_info) override;
  void OnError(device::mojom::SmartCardError error) override;

 private:
  struct PendingConnectCall {
    PendingConnectCall(std::string reader,
                       device::mojom::SmartCardShareMode share_mode,
                       device::mojom::SmartCardProtocolsPtr preferred_protocols,
                       SmartCardService::ConnectCallback callback);
    PendingConnectCall(PendingConnectCall&&);
    virtual ~PendingConnectCall();
    std::string reader;
    device::mojom::SmartCardShareMode share_mode;
    device::mojom::SmartCardProtocolsPtr preferred_protocols;
    SmartCardService::ConnectCallback callback;
  };

  void OnCreateContextDone(
      device::mojom::SmartCardCreateContextResultPtr result);
  void OnConnectDone(ConnectCallback callback,
                     device::mojom::SmartCardConnectResultPtr result);
  void IssuePendingConnectCalls();
  void FailPendingConnectCalls(device::mojom::SmartCardError error);

  const raw_ref<SmartCardReaderTracker> reader_tracker_;
  mojo::Remote<device::mojom::SmartCardContextFactory> context_factory_;

  // There are three possible states:
  // 1. !context_.has_value()
  //   context_factory_->CreateContext() must be called.
  // 2. !context_->is_bound()
  //   context_factory_->CreateContext() was called but it hasn't finished yet.
  // 3. context_->is_bound()
  //   It's ready to be used.
  absl::optional<mojo::Remote<device::mojom::SmartCardContext>> context_;

  const bool supports_reader_added_removed_notifications_;

  base::queue<PendingConnectCall> pending_connect_calls_;

  // Used to bind with Blink.
  mojo::AssociatedRemoteSet<blink::mojom::SmartCardServiceClient> clients_;

  base::WeakPtrFactory<SmartCardService> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_
