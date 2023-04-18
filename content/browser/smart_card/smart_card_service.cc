// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/smart_card_service.h"

#include "content/browser/renderer_host/isolated_context_util.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/smart_card_delegate.h"
#include "services/device/public/mojom/smart_card.mojom.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

namespace {

// Deletes the SmartCardService when the connected document is destroyed.
class DocumentHelper
    : public content::DocumentService<blink::mojom::SmartCardService> {
 public:
  DocumentHelper(std::unique_ptr<SmartCardService> service,
                 RenderFrameHost& render_frame_host,
                 mojo::PendingReceiver<blink::mojom::SmartCardService> receiver)
      : DocumentService(render_frame_host, std::move(receiver)),
        service_(std::move(service)) {
    DCHECK(service_);
  }
  ~DocumentHelper() override = default;

  // blink::mojom::SmartCardService:
  void GetReadersAndStartTracking(
      GetReadersAndStartTrackingCallback callback) override {
    service_->GetReadersAndStartTracking(std::move(callback));
  }

  void RegisterClient(mojo::PendingAssociatedRemote<
                          blink::mojom::SmartCardServiceClient> client,
                      RegisterClientCallback callback) override {
    service_->RegisterClient(std::move(client), std::move(callback));
  }

  void Connect(const std::string& reader,
               device::mojom::SmartCardShareMode share_mode,
               device::mojom::SmartCardProtocolsPtr preferred_protocols,
               ConnectCallback callback) override {
    service_->Connect(reader, share_mode, std::move(preferred_protocols),
                      std::move(callback));
  }

 private:
  const std::unique_ptr<SmartCardService> service_;
};

}  // namespace

SmartCardService::SmartCardService(
    mojo::PendingRemote<device::mojom::SmartCardContextFactory> context_factory,
    bool supports_reader_added_removed_notifications,
    SmartCardReaderTracker& reader_tracker)
    : reader_tracker_(reader_tracker),
      context_factory_(std::move(context_factory)),
      supports_reader_added_removed_notifications_(
          supports_reader_added_removed_notifications) {}

SmartCardService::~SmartCardService() {
  reader_tracker_->Stop(this);
}

// static
void SmartCardService::Create(
    RenderFrameHostImpl* render_frame_host,
    mojo::PendingReceiver<blink::mojom::SmartCardService> receiver) {
  BrowserContext* browser_context = render_frame_host->GetBrowserContext();
  DCHECK(browser_context);

  if (!base::FeatureList::IsEnabled(blink::features::kSmartCard)) {
    mojo::ReportBadMessage("The SmartCard feature is disabled.");
    return;
  }

  if (!render_frame_host->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kSmartCard)) {
    mojo::ReportBadMessage(
        "Access to the feature \"smart-card\" is disallowed by permissions "
        "policy.");
    return;
  }

  if (!IsFrameSufficientlyIsolated(render_frame_host)) {
    mojo::ReportBadMessage(
        "Frame is not sufficiently isolated to use the Smart Card API.");
    return;
  }

  SmartCardDelegate* delegate =
      GetContentClient()->browser()->GetSmartCardDelegate(browser_context);
  if (!delegate) {
    mojo::ReportBadMessage("Browser has no Smart Card delegate.");
    return;
  }

  SmartCardReaderTracker& reader_tracker =
      SmartCardReaderTracker::GetForBrowserContext(*browser_context, *delegate);

  // DocumentHelper observes the lifetime of the document connected to
  // `render_frame_host` and destroys the SmartCardService when the Mojo
  // connection is disconnected, RenderFrameHost is deleted, or the
  // RenderFrameHost commits a cross-document navigation. It forwards its Mojo
  // interface to SmartCardService.
  new DocumentHelper(
      std::make_unique<SmartCardService>(
          delegate->GetSmartCardContextFactory(*browser_context),
          delegate->SupportsReaderAddedRemovedNotifications(), reader_tracker),
      *render_frame_host, std::move(receiver));
}

void SmartCardService::GetReadersAndStartTracking(
    GetReadersAndStartTrackingCallback callback) {
  reader_tracker_->Start(this, std::move(callback));
}

void SmartCardService::RegisterClient(
    mojo::PendingAssociatedRemote<blink::mojom::SmartCardServiceClient> client,
    RegisterClientCallback callback) {
  clients_.Add(std::move(client));

  std::move(callback).Run(supports_reader_added_removed_notifications_);
}

void SmartCardService::Connect(
    const std::string& reader,
    device::mojom::SmartCardShareMode share_mode,
    device::mojom::SmartCardProtocolsPtr preferred_protocols,
    ConnectCallback callback) {
  if (!context_) {
    context_ = mojo::Remote<device::mojom::SmartCardContext>();
    context_factory_->CreateContext(
        base::BindOnce(&SmartCardService::OnCreateContextDone,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (!context_->is_bound()) {
    // Still waiting for the response of a CreateContext() call.
    pending_connect_calls_.emplace(reader, share_mode,
                                   std::move(preferred_protocols),
                                   std::move(callback));
    return;
  }

  context_.value()->Connect(
      reader, share_mode, std::move(preferred_protocols),
      base::BindOnce(&SmartCardService::OnConnectDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SmartCardService::OnReaderAdded(
    const blink::mojom::SmartCardReaderInfo& reader_info) {
  for (auto& client : clients_) {
    client->ReaderAdded(reader_info.Clone());
  }
}

void SmartCardService::OnReaderRemoved(
    const blink::mojom::SmartCardReaderInfo& reader_info) {
  for (auto& client : clients_) {
    client->ReaderRemoved(reader_info.Clone());
  }
}

void SmartCardService::OnReaderChanged(
    const blink::mojom::SmartCardReaderInfo& reader_info) {
  for (auto& client : clients_) {
    client->ReaderChanged(reader_info.Clone());
  }
}

void SmartCardService::OnError(device::mojom::SmartCardError error) {
  for (auto& client : clients_) {
    client->Error(error);
  }
}

void SmartCardService::OnCreateContextDone(
    device::mojom::SmartCardCreateContextResultPtr result) {
  CHECK(context_ && !context_->is_bound());

  if (result->is_error()) {
    context_.reset();
    FailPendingConnectCalls(result->get_error());
    return;
  }

  context_->Bind(std::move(result->get_context()));
  IssuePendingConnectCalls();
}

void SmartCardService::OnConnectDone(
    ConnectCallback callback,
    device::mojom::SmartCardConnectResultPtr result) {
  CHECK(context_ && context_->is_bound());

  if (result->is_error()) {
    const device::mojom::SmartCardError error = result->get_error();
    if (error == device::mojom::SmartCardError::kInvalidHandle ||
        error == device::mojom::SmartCardError::kNoService) {
      // Those are unrecoverable errors that mean the context is useless.
      context_.reset();
      std::move(callback).Run(std::move(result));
      FailPendingConnectCalls(error);
      return;
    }
  }

  std::move(callback).Run(std::move(result));
}

void SmartCardService::IssuePendingConnectCalls() {
  CHECK(context_ && context_->is_bound());

  while (!pending_connect_calls_.empty()) {
    auto pending_connect = std::move(pending_connect_calls_.front());
    pending_connect_calls_.pop();

    context_.value()->Connect(
        pending_connect.reader, pending_connect.share_mode,
        std::move(pending_connect.preferred_protocols),
        base::BindOnce(&SmartCardService::OnConnectDone,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(pending_connect.callback)));
  }
}

void SmartCardService::FailPendingConnectCalls(
    device::mojom::SmartCardError error) {
  while (!pending_connect_calls_.empty()) {
    auto pending_connect = std::move(pending_connect_calls_.front());
    pending_connect_calls_.pop();
    std::move(pending_connect.callback)
        .Run(device::mojom::SmartCardConnectResult::NewError(error));
  }
}

SmartCardService::PendingConnectCall::~PendingConnectCall() = default;
SmartCardService::PendingConnectCall::PendingConnectCall(PendingConnectCall&&) =
    default;
SmartCardService::PendingConnectCall::PendingConnectCall(
    std::string reader,
    device::mojom::SmartCardShareMode share_mode,
    device::mojom::SmartCardProtocolsPtr preferred_protocols,
    SmartCardService::ConnectCallback callback)
    : reader(std::move(reader)),
      share_mode(share_mode),
      preferred_protocols(std::move(preferred_protocols)),
      callback(std::move(callback)) {}

}  // namespace content
