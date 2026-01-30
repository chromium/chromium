// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/smart_card_emulation_handler.h"

#include <optional>

#include "base/uuid.h"
#include "content/browser/devtools/protocol/smart_card_emulation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/smart_card/smart_card_service.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/smart_card_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"

namespace content::protocol {

namespace {
std::string GenerateRequestId() {
  return base::Uuid::GenerateRandomV4().AsLowercaseString();
}

// Convert binary data (std::vector<uint8_t>) to Protocol Binary.
protocol::Binary ToProtocolBinary(const std::vector<uint8_t>& data) {
  return protocol::Binary::fromSpan(std::move(data));
}

// Convert Mojo Disposition enum to Protocol String.
String ToProtocolDisposition(device::mojom::SmartCardDisposition disposition) {
  switch (disposition) {
    case device::mojom::SmartCardDisposition::kLeave:
      return SmartCardEmulation::DispositionEnum::LeaveCard;
    case device::mojom::SmartCardDisposition::kReset:
      return SmartCardEmulation::DispositionEnum::ResetCard;
    case device::mojom::SmartCardDisposition::kUnpower:
      return SmartCardEmulation::DispositionEnum::UnpowerCard;
    case device::mojom::SmartCardDisposition::kEject:
      return SmartCardEmulation::DispositionEnum::EjectCard;
  }
  // The 'default' case is intentionally omitted to ensure a compile-time error
  // if a new value is added to the SmartCardDisposition enum.
  NOTREACHED();
}

// Convert Mojo SmartCardReaderStateFlags to Protocol reader state flags.
std::unique_ptr<SmartCardEmulation::ReaderStateFlags>
ToProtocolReaderStateFlags(
    const device::mojom::SmartCardReaderStateFlags& flags) {
  return SmartCardEmulation::ReaderStateFlags::Create()
      .SetUnaware(flags.unaware)
      .SetIgnore(flags.ignore)
      .SetChanged(flags.changed)
      .SetUnknown(flags.unknown)
      .SetUnavailable(flags.unavailable)
      .SetEmpty(flags.empty)
      .SetPresent(flags.present)
      .SetExclusive(flags.exclusive)
      .SetInuse(flags.inuse)
      .SetMute(flags.mute)
      .SetUnpowered(flags.unpowered)
      .Build();
}

// Convert Mojo ReaderStates to Protocol Array.
std::unique_ptr<protocol::Array<protocol::SmartCardEmulation::ReaderStateIn>>
ToProtocolReaderStates(
    const std::vector<device::mojom::SmartCardReaderStateInPtr>&
        reader_states) {
  auto protocol_states = std::make_unique<
      protocol::Array<protocol::SmartCardEmulation::ReaderStateIn>>();

  for (const auto& rs : reader_states) {
    auto in =
        SmartCardEmulation::ReaderStateIn::Create()
            .SetReader(rs->reader)
            .SetCurrentInsertionCount(rs->current_count)
            .SetCurrentState(ToProtocolReaderStateFlags(*rs->current_state))
            .Build();
    protocol_states->push_back(std::move(in));
  }
  return protocol_states;
}

// Convert Mojo Enum to Protocol String.
std::string ToProtocolShareMode(device::mojom::SmartCardShareMode share_mode) {
  switch (share_mode) {
    case device::mojom::SmartCardShareMode::kShared:
      return SmartCardEmulation::ShareModeEnum::Shared;
    case device::mojom::SmartCardShareMode::kExclusive:
      return SmartCardEmulation::ShareModeEnum::Exclusive;
    case device::mojom::SmartCardShareMode::kDirect:
      return SmartCardEmulation::ShareModeEnum::Direct;
  }
  // The 'default' case is intentionally omitted to ensure a compile-time error
  // if a new value is added to the SmartCardShareMode enum.
  NOTREACHED();
}

// Convert Mojo Struct to ProtocolSet Object.
std::unique_ptr<protocol::SmartCardEmulation::ProtocolSet>
ToProtocolProtocolSet(const device::mojom::SmartCardProtocolsPtr& protocols) {
  auto builder = protocol::SmartCardEmulation::ProtocolSet::Create();
  if (protocols) {
    builder.SetT0(protocols->t0);
    builder.SetT1(protocols->t1);
    builder.SetRaw(protocols->raw);
  } else {
    builder.SetT0(false);
    builder.SetT1(false);
    builder.SetRaw(false);
  }
  return builder.Build();
}

// Convert Mojo Protocol to Protocol SmartCardProtocol.
std::optional<std::string> ToProtocolSmartCardProtocol(
    device::mojom::SmartCardProtocol protocol) {
  switch (protocol) {
    case device::mojom::SmartCardProtocol::kT0:
      return protocol::SmartCardEmulation::ProtocolEnum::T0;
    case device::mojom::SmartCardProtocol::kT1:
      return protocol::SmartCardEmulation::ProtocolEnum::T1;
    case device::mojom::SmartCardProtocol::kRaw:
      return protocol::SmartCardEmulation::ProtocolEnum::Raw;
    case device::mojom::SmartCardProtocol::kUndefined:
      return std::nullopt;
  }
  // The 'default' case is intentionally omitted to ensure a compile-time error
  // if a new value is added to the mojo SmartCardProtocol enum.
  NOTREACHED();
}

}  // namespace

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

void SmartCardEmulationHandler::AddPendingRequest(const std::string& request_id,
                                                  PendingRequest request) {
  pending_requests_.emplace(request_id, std::move(request));
}

template <typename T>
base::expected<T, std::string> SmartCardEmulationHandler::TakePendingRequest(
    const std::string& request_id) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    return base::unexpected("Request ID not found");
  }

  // Check if the variant holds the specific template instantiation T.
  T* request_ptr = std::get_if<T>(&it->second);
  if (!request_ptr) {
    return base::unexpected("Request type mismatch");
  }

  T request = std::move(*request_ptr);
  pending_requests_.erase(it);
  return request;
}

void SmartCardEmulationHandler::OnCreateContext(
    device::mojom::SmartCardContextFactory::CreateContextCallback callback) {
  std::string request_id = GenerateRequestId();
  AddPendingRequest(request_id, PendingCreateContext(std::move(callback)));
  frontend_->EstablishContextRequested(request_id);
}

void SmartCardEmulationHandler::OnListReaders(
    uint32_t context_id,
    device::mojom::SmartCardContext::ListReadersCallback callback) {
  std::string request_id = GenerateRequestId();
  AddPendingRequest(request_id, PendingListReaders(std::move(callback)));
  frontend_->ListReadersRequested(request_id, context_id);
}

void SmartCardEmulationHandler::OnGetStatusChange(
    uint32_t context_id,
    base::TimeDelta timeout,
    std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states,
    device::mojom::SmartCardContext::GetStatusChangeCallback callback) {
  std::string request_id = GenerateRequestId();
  AddPendingRequest(request_id, PendingGetStatusChange(std::move(callback)));
  frontend_->GetStatusChangeRequested(request_id, context_id,
                                      ToProtocolReaderStates(reader_states),
                                      timeout.InMilliseconds());
}

void SmartCardEmulationHandler::OnCancel(
    uint32_t context_id,
    device::mojom::SmartCardContext::CancelCallback callback) {
  std::string request_id = GenerateRequestId();
  AddPendingRequest(request_id, PendingPlainResult(std::move(callback)));
  frontend_->CancelRequested(request_id, context_id);
}

void SmartCardEmulationHandler::OnConnect(
    uint32_t context_id,
    const std::string& reader,
    device::mojom::SmartCardShareMode share_mode,
    device::mojom::SmartCardProtocolsPtr preferred_protocols,
    mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher> watcher,
    device::mojom::SmartCardContext::ConnectCallback callback) {
  std::string request_id = GenerateRequestId();

  AddPendingRequest(request_id, PendingConnect(std::move(callback)));

  frontend_->ConnectRequested(
      request_id, context_id, reader, ToProtocolShareMode(share_mode),
      ToProtocolProtocolSet(std::move(preferred_protocols)));
}

void SmartCardEmulationHandler::OnDisconnect(
    uint32_t handle,
    device::mojom::SmartCardDisposition disposition,
    device::mojom::SmartCardConnection::DisconnectCallback callback) {
  std::string request_id = GenerateRequestId();
  AddPendingRequest(request_id, PendingPlainResult(std::move(callback)));
  frontend_->DisconnectRequested(request_id, handle,
                                 ToProtocolDisposition(disposition));
}

void SmartCardEmulationHandler::OnControl(
    uint32_t handle,
    uint32_t control_code,
    const std::vector<uint8_t>& data,
    device::mojom::SmartCardConnection::ControlCallback callback) {
  std::string request_id = GenerateRequestId();
  AddPendingRequest(request_id, PendingDataResult(std::move(callback)));
  frontend_->ControlRequested(request_id, handle, control_code,
                              ToProtocolBinary(data));
}

void SmartCardEmulationHandler::OnGetAttrib(
    uint32_t handle,
    uint32_t id,
    device::mojom::SmartCardConnection::GetAttribCallback callback) {
  std::string request_id = GenerateRequestId();

  AddPendingRequest(request_id, PendingDataResult(std::move(callback)));

  frontend_->GetAttribRequested(request_id, handle, id);
}

void SmartCardEmulationHandler::OnTransmit(
    uint32_t handle,
    device::mojom::SmartCardProtocol protocol,
    const std::vector<uint8_t>& data,
    device::mojom::SmartCardConnection::TransmitCallback callback) {
  std::string request_id = GenerateRequestId();
  AddPendingRequest(request_id, PendingDataResult(std::move(callback)));
  frontend_->TransmitRequested(request_id, handle, ToProtocolBinary(data),
                               ToProtocolSmartCardProtocol(protocol));
}

void SmartCardEmulationHandler::OnStatus(
    uint32_t handle,
    device::mojom::SmartCardConnection::StatusCallback callback) {
  std::string request_id = GenerateRequestId();

  AddPendingRequest(request_id, PendingStatus(std::move(callback)));

  frontend_->StatusRequested(request_id, handle);
}

void SmartCardEmulationHandler::OnSetAttrib(
    uint32_t handle,
    uint32_t id,
    const std::vector<uint8_t>& data,
    device::mojom::SmartCardConnection::SetAttribCallback callback) {
  std::string request_id = GenerateRequestId();
  AddPendingRequest(request_id, PendingPlainResult(std::move(callback)));
  frontend_->SetAttribRequested(request_id, handle, id,
                                std::move(ToProtocolBinary(data)));
}

void SmartCardEmulationHandler::OnBeginTransaction(
    uint32_t handle,
    device::mojom::SmartCardConnection::BeginTransactionCallback callback) {
  std::string request_id = GenerateRequestId();

  AddPendingRequest(request_id, PendingBeginTransaction(std::move(callback)));

  frontend_->BeginTransactionRequested(request_id, handle);
}

void SmartCardEmulationHandler::OnEndTransaction(
    uint32_t handle,
    device::mojom::SmartCardDisposition disposition,
    device::mojom::SmartCardTransaction::EndTransactionCallback callback) {
  std::string request_id = GenerateRequestId();
  AddPendingRequest(request_id, PendingPlainResult(std::move(callback)));
  frontend_->EndTransactionRequested(request_id, handle,
                                     ToProtocolDisposition(disposition));
}

}  // namespace content::protocol
