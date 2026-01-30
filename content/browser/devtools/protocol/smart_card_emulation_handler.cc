// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/smart_card_emulation_handler.h"

#include "base/uuid.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/smart_card/smart_card_service.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/smart_card_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"

namespace content::protocol {

SmartCardEmulationHandler::SmartCardEmulationHandler(
    content::WebContents* web_contents)
    : DevToolsDomainHandler(SmartCardEmulation::Metainfo::domainName),
      content::WebContentsObserver(web_contents) {}

SmartCardEmulationHandler::~SmartCardEmulationHandler() = default;

// static
std::vector<SmartCardEmulationHandler*> SmartCardEmulationHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<SmartCardEmulationHandler>(
      SmartCardEmulation::Metainfo::domainName);
}

void SmartCardEmulationHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = new SmartCardEmulation::Frontend(dispatcher->channel());
  SmartCardEmulation::Dispatcher::wire(dispatcher, this);
}

void SmartCardEmulationHandler::SetRenderer(int process_host_id,
                                            RenderFrameHostImpl* frame_host) {
  // If frame_host changes, update the observer target.
  Observe(content::WebContents::FromRenderFrameHost(frame_host));
}

void SmartCardEmulationHandler::ReadyToCommitNavigation(
    content::NavigationHandle* handle) {
  // Automatically re-apply emulation when the frame changes.
  if (IsEnabled() && handle->IsInPrimaryMainFrame() && !handle->IsErrorPage()) {
    UpdateEmulationOverride(handle->GetRenderFrameHost());
  }
}

void SmartCardEmulationHandler::UpdateEmulationOverride(
    content::RenderFrameHost* rfh) {
  CHECK(IsEnabled());
  CHECK(rfh);

  auto* delegate =
      content::GetContentClient()->browser()->GetSmartCardDelegate();
  if (!delegate) {
    return;
  }

  auto factory_getter = base::BindRepeating(
      [](base::WeakPtr<EmulatedSmartCardContextFactory> factory) {
        mojo::PendingRemote<device::mojom::SmartCardContextFactory> remote;
        if (factory) {
          factory->BindReceiver(remote.InitWithNewPipeAndPassReceiver());
        }
        return remote;
      },
      factory_->GetWeakPtr());

  delegate->SetEmulationFactory(rfh->GetGlobalId(), std::move(factory_getter));
}

DispatchResponse SmartCardEmulationHandler::Enable() {
  if (IsEnabled()) {
    return DispatchResponse::Success();
  }

  if (!content::GetContentClient()->browser()->GetSmartCardDelegate()) {
    return DispatchResponse::ServerError(
        "Smart Card emulation is not supported on this platform.");
  }

  if (!web_contents()) {
    return DispatchResponse::ServerError("WebContents not available");
  }

  factory_ = std::make_unique<EmulatedSmartCardContextFactory>(*this);

  UpdateEmulationOverride(web_contents()->GetPrimaryMainFrame());
  return DispatchResponse::Success();
}

DispatchResponse SmartCardEmulationHandler::Disable() {
  if (!IsEnabled()) {
    return DispatchResponse::Success();
  }

  if (web_contents()) {
    content::SmartCardDelegate* delegate =
        content::GetContentClient()->browser()->GetSmartCardDelegate();
    if (delegate) {
      delegate->ClearEmulationFactory(
          web_contents()->GetPrimaryMainFrame()->GetGlobalId());
    }
  }

  factory_.reset();

  return DispatchResponse::Success();
}

void SmartCardEmulationHandler::OnCreateContext(
    device::mojom::SmartCardContextFactory::CreateContextCallback callback) {}

void SmartCardEmulationHandler::OnListReaders(
    uint32_t context_id,
    device::mojom::SmartCardContext::ListReadersCallback callback) {}

void SmartCardEmulationHandler::OnGetStatusChange(
    uint32_t context_id,
    base::TimeDelta timeout,
    std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states,
    device::mojom::SmartCardContext::GetStatusChangeCallback callback) {}

void SmartCardEmulationHandler::OnCancel(
    uint32_t context_id,
    device::mojom::SmartCardContext::CancelCallback callback) {}

void SmartCardEmulationHandler::OnConnect(
    uint32_t context_id,
    const std::string& reader,
    device::mojom::SmartCardShareMode share_mode,
    device::mojom::SmartCardProtocolsPtr preferred_protocols,
    mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher> watcher,
    device::mojom::SmartCardContext::ConnectCallback callback) {}

void SmartCardEmulationHandler::OnDisconnect(
    uint32_t handle,
    device::mojom::SmartCardDisposition disposition,
    device::mojom::SmartCardConnection::DisconnectCallback callback) {}

void SmartCardEmulationHandler::OnControl(
    uint32_t handle,
    uint32_t control_code,
    const std::vector<uint8_t>& data,
    device::mojom::SmartCardConnection::ControlCallback callback) {}

void SmartCardEmulationHandler::OnGetAttrib(
    uint32_t handle,
    uint32_t id,
    device::mojom::SmartCardConnection::GetAttribCallback callback) {}

void SmartCardEmulationHandler::OnTransmit(
    uint32_t handle,
    device::mojom::SmartCardProtocol protocol,
    const std::vector<uint8_t>& data,
    device::mojom::SmartCardConnection::TransmitCallback callback) {}

void SmartCardEmulationHandler::OnStatus(
    uint32_t handle,
    device::mojom::SmartCardConnection::StatusCallback callback) {}

void SmartCardEmulationHandler::OnSetAttrib(
    uint32_t handle,
    uint32_t id,
    const std::vector<uint8_t>& data,
    device::mojom::SmartCardConnection::SetAttribCallback callback) {}

void SmartCardEmulationHandler::OnBeginTransaction(
    uint32_t handle,
    device::mojom::SmartCardConnection::BeginTransactionCallback callback) {}

void SmartCardEmulationHandler::OnEndTransaction(
    uint32_t handle,
    device::mojom::SmartCardDisposition disposition,
    device::mojom::SmartCardTransaction::EndTransactionCallback callback) {}

}  // namespace content::protocol
