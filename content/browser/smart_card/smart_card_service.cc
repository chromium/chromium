// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/smart_card_service.h"

#include "base/check_deref.h"
#include "base/containers/map_util.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/isolated_context_util.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/smart_card_delegate.h"
#include "services/device/public/mojom/smart_card.mojom.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

using device::mojom::SmartCardConnectResult;
using device::mojom::SmartCardCreateContextResult;
using device::mojom::SmartCardError;

namespace {
SmartCardDelegate& GetSmartCardDelegate() {
  return CHECK_DEREF(GetContentClient()->browser()->GetSmartCardDelegate());
}
}  // namespace

SmartCardService::SmartCardService(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::SmartCardService> receiver,
    mojo::PendingRemote<device::mojom::SmartCardContextFactory> context_factory)
    : DocumentService(render_frame_host, std::move(receiver)),
      context_factory_(std::move(context_factory)) {
  context_wrapper_receivers_.set_disconnect_handler(
      base::BindRepeating(&SmartCardService::OnMojoWrapperContextDisconnected,
                          base::Unretained(this)));
}

SmartCardService::~SmartCardService() {}

// static
void SmartCardService::Create(
    RenderFrameHost* render_frame_host,
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

  if (!HasIsolatedContextCapability(render_frame_host)) {
    mojo::ReportBadMessage(
        "Frame is not sufficiently isolated to use the Smart Card API.");
    return;
  }

  SmartCardDelegate* delegate =
      GetContentClient()->browser()->GetSmartCardDelegate();
  if (!delegate) {
    mojo::ReportBadMessage("Browser has no Smart Card delegate.");
    return;
  }

  new SmartCardService(*render_frame_host, std::move(receiver),
                       delegate->GetSmartCardContextFactory(*browser_context));
}

void SmartCardService::CreateContext(CreateContextCallback callback) {
  if (GetSmartCardDelegate().IsPermissionBlocked(render_frame_host())) {
    std::move(callback).Run(SmartCardCreateContextResult::NewError(
        SmartCardError::kPermissionDenied));
    return;
  }

  context_factory_->CreateContext(
      base::BindOnce(&SmartCardService::OnContextCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SmartCardService::ListReaders(ListReadersCallback callback) {
  mojo::ReceiverId context_wrapper_id =
      context_wrapper_receivers_.current_receiver();

  mojo::Remote<SmartCardContext>& context_remote =
      CHECK_DEREF(base::FindOrNull(context_remotes_, context_wrapper_id));

  context_remote->ListReaders(
      base::BindOnce(&SmartCardService::OnListReadersResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SmartCardService::GetStatusChange(
    base::TimeDelta timeout,
    std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states,
    GetStatusChangeCallback callback) {
  mojo::ReceiverId context_wrapper_id =
      context_wrapper_receivers_.current_receiver();

  mojo::Remote<SmartCardContext>& context_remote =
      CHECK_DEREF(base::FindOrNull(context_remotes_, context_wrapper_id));

  context_remote->GetStatusChange(timeout, std::move(reader_states),
                                  std::move(callback));
}

void SmartCardService::Cancel(CancelCallback callback) {
  mojo::ReceiverId context_wrapper_id =
      context_wrapper_receivers_.current_receiver();

  mojo::Remote<SmartCardContext>& context_remote =
      CHECK_DEREF(base::FindOrNull(context_remotes_, context_wrapper_id));

  context_remote->Cancel(std::move(callback));
}

void SmartCardService::Connect(
    const std::string& reader,
    device::mojom::SmartCardShareMode share_mode,
    device::mojom::SmartCardProtocolsPtr preferred_protocols,
    ConnectCallback callback) {
  SmartCardDelegate& delegate = GetSmartCardDelegate();

  mojo::ReceiverId context_wrapper_id =
      context_wrapper_receivers_.current_receiver();

  if (delegate.HasReaderPermission(render_frame_host(), reader)) {
    mojo::Remote<SmartCardContext>& context_remote =
        CHECK_DEREF(base::FindOrNull(context_remotes_, context_wrapper_id));

    context_remote->Connect(reader, share_mode, std::move(preferred_protocols),
                            std::move(callback));
    return;
  }

  if (!valid_reader_names_.contains(reader)) {
    // Avoid showing the user a string that comes directly from the application.
    //
    // This will also block the case where an application asks to connect to a
    // reader without first checking whether it exists in the system (via
    // ListReaders). But no sane application should be doing this anyway.  If
    // this turns out to be a problem we will have to do a ListReaders() here on
    // our own before coming to a decision.
    std::move(callback).Run(
        SmartCardConnectResult::NewError(SmartCardError::kPermissionDenied));
    return;
  }

  delegate.RequestReaderPermission(
      render_frame_host(), reader,
      base::BindOnce(&SmartCardService::OnReaderPermissionResult,
                     weak_ptr_factory_.GetWeakPtr(), context_wrapper_id, reader,
                     share_mode, std::move(preferred_protocols),
                     std::move(callback)));
}

void SmartCardService::OnReaderPermissionResult(
    mojo::ReceiverId context_wrapper_id,
    const std::string& reader,
    device::mojom::SmartCardShareMode share_mode,
    device::mojom::SmartCardProtocolsPtr preferred_protocols,
    ConnectCallback callback,
    bool granted) {
  auto it = context_remotes_.find(context_wrapper_id);
  if (it == context_remotes_.end()) {
    // Can happen if the renderer has dropped the wrapper remote in the
    // meantime.
    std::move(callback).Run(
        SmartCardConnectResult::NewError(SmartCardError::kUnexpected));
    return;
  }

  if (!granted) {
    std::move(callback).Run(
        SmartCardConnectResult::NewError(SmartCardError::kPermissionDenied));
    return;
  }

  mojo::Remote<SmartCardContext>& context_remote = it->second;

  context_remote->Connect(reader, share_mode, std::move(preferred_protocols),
                          std::move(callback));
}

void SmartCardService::OnMojoWrapperContextDisconnected() {
  mojo::ReceiverId context_wrapper_id =
      context_wrapper_receivers_.current_receiver();

  context_remotes_.erase(context_wrapper_id);
}

void SmartCardService::OnListReadersResult(
    ListReadersCallback callback,
    device::mojom::SmartCardListReadersResultPtr result) {
  if (result->is_readers()) {
    // Update our set of valid reader names.
    // Note that this is larger than the set of "currently available readers".
    for (const auto& reader : result->get_readers()) {
      valid_reader_names_.insert(reader);
    }
  }

  // And finally forward the result to the original caller.
  std::move(callback).Run(std::move(result));
}

void SmartCardService::OnContextCreated(
    CreateContextCallback callback,
    ::device::mojom::SmartCardCreateContextResultPtr result) {
  if (result->is_error()) {
    std::move(callback).Run(std::move(result));
    return;
  }

  // Wrap the context so that we can do permission checking/prompting before
  // forwarding a call where appropriate.

  mojo::PendingRemote<device::mojom::SmartCardContext> wrapper_remote;

  mojo::ReceiverId context_wrapper_id = context_wrapper_receivers_.Add(
      this, wrapper_remote.InitWithNewPipeAndPassReceiver());

  context_remotes_[context_wrapper_id] =
      mojo::Remote<SmartCardContext>(std::move(result->get_context()));

  result->set_context(std::move(wrapper_remote));

  std::move(callback).Run(std::move(result));
}

}  // namespace content
