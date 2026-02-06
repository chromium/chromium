// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/smart_card_emulation_handler.h"

#include <optional>

#include "base/types/expected_macros.h"
#include "base/uuid.h"
#include "content/browser/devtools/protocol/smart_card_emulation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/smart_card/emulation/emulated_smart_card_connection.h"
#include "content/browser/smart_card/emulation/emulated_smart_card_context.h"
#include "content/browser/smart_card/emulation/emulated_smart_card_transaction.h"
#include "content/browser/smart_card/smart_card_service.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/smart_card_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "services/device/public/mojom/smart_card.mojom.h"

namespace content::protocol {

namespace {
std::string GenerateRequestId() {
  return base::Uuid::GenerateRandomV4().AsLowercaseString();
}

// Convert binary data (std::vector<uint8_t>) to Protocol Binary.
protocol::Binary ToProtocolBinary(const std::vector<uint8_t>& data) {
  return protocol::Binary::fromSpan(std::move(data));
}

// Convert Protocol Binary to binary data (std::vector<uint8_t>).
std::vector<uint8_t> ToVector(const protocol::Binary& binary) {
  return std::vector<uint8_t>(binary.begin(), binary.end());
}

// Convert the Protocol error string to the Mojo error enum.
device::mojom::SmartCardError ToMojoSmartCardError(
    const std::string& result_code) {
  namespace ResultCodeEnum = protocol::SmartCardEmulation::ResultCodeEnum;

  static const base::NoDestructor<
      base::flat_map<std::string, device::mojom::SmartCardError>>
      kErrorMap({
          {ResultCodeEnum::RemovedCard,
           device::mojom::SmartCardError::kRemovedCard},
          {ResultCodeEnum::ResetCard,
           device::mojom::SmartCardError::kResetCard},
          {ResultCodeEnum::UnpoweredCard,
           device::mojom::SmartCardError::kUnpoweredCard},
          {ResultCodeEnum::UnresponsiveCard,
           device::mojom::SmartCardError::kUnresponsiveCard},
          {ResultCodeEnum::UnsupportedCard,
           device::mojom::SmartCardError::kUnsupportedCard},
          {ResultCodeEnum::ReaderUnavailable,
           device::mojom::SmartCardError::kReaderUnavailable},
          {ResultCodeEnum::SharingViolation,
           device::mojom::SmartCardError::kSharingViolation},
          {ResultCodeEnum::NotTransacted,
           device::mojom::SmartCardError::kNotTransacted},
          {ResultCodeEnum::NoSmartcard,
           device::mojom::SmartCardError::kNoSmartcard},
          {ResultCodeEnum::ProtoMismatch,
           device::mojom::SmartCardError::kProtoMismatch},
          {ResultCodeEnum::SystemCancelled,
           device::mojom::SmartCardError::kSystemCancelled},
          {ResultCodeEnum::NotReady, device::mojom::SmartCardError::kNotReady},
          {ResultCodeEnum::Cancelled,
           device::mojom::SmartCardError::kCancelled},
          {ResultCodeEnum::InsufficientBuffer,
           device::mojom::SmartCardError::kInsufficientBuffer},
          {ResultCodeEnum::InvalidHandle,
           device::mojom::SmartCardError::kInvalidHandle},
          {ResultCodeEnum::InvalidParameter,
           device::mojom::SmartCardError::kInvalidParameter},
          {ResultCodeEnum::InvalidValue,
           device::mojom::SmartCardError::kInvalidValue},
          {ResultCodeEnum::NoMemory, device::mojom::SmartCardError::kNoMemory},
          {ResultCodeEnum::Timeout, device::mojom::SmartCardError::kTimeout},
          {ResultCodeEnum::UnknownReader,
           device::mojom::SmartCardError::kUnknownReader},
          {ResultCodeEnum::UnsupportedFeature,
           device::mojom::SmartCardError::kUnsupportedFeature},
          {ResultCodeEnum::NoReadersAvailable,
           device::mojom::SmartCardError::kNoReadersAvailable},
          {ResultCodeEnum::ServiceStopped,
           device::mojom::SmartCardError::kServiceStopped},
          {ResultCodeEnum::NoService,
           device::mojom::SmartCardError::kNoService},
          {ResultCodeEnum::CommError,
           device::mojom::SmartCardError::kCommError},
          {ResultCodeEnum::InternalError,
           device::mojom::SmartCardError::kInternalError},
          {ResultCodeEnum::ServerTooBusy,
           device::mojom::SmartCardError::kServerTooBusy},
          {ResultCodeEnum::Unexpected,
           device::mojom::SmartCardError::kUnexpected},
          {ResultCodeEnum::Shutdown, device::mojom::SmartCardError::kShutdown},
          {ResultCodeEnum::UnknownCard,
           device::mojom::SmartCardError::kUnknownError},
          {ResultCodeEnum::Unknown, device::mojom::SmartCardError::kUnknown},
      });

  auto it = kErrorMap->find(result_code);
  if (it != kErrorMap->end()) {
    return it->second;
  }

  // The DevTools dispatcher guarantees that 'result_code' matches one of the
  // string literals defined in the PDL (ResultCodeEnum), so this branch
  // is unreachable unless the PDL and this map are out of sync.
  NOTREACHED();
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

// Convert Pdl Reader State Flags to mojo.
device::mojom::SmartCardReaderStateFlagsPtr ToMojoReaderStateFlags(
    protocol::SmartCardEmulation::ReaderStateFlags* flags) {
  auto mojo_flags = device::mojom::SmartCardReaderStateFlags::New();

  if (!flags) {
    return mojo_flags;
  }

  mojo_flags->unaware = flags->GetUnaware(false);
  mojo_flags->ignore = flags->GetIgnore(false);
  mojo_flags->changed = flags->GetChanged(false);
  mojo_flags->unknown = flags->GetUnknown(false);
  mojo_flags->unavailable = flags->GetUnavailable(false);
  mojo_flags->empty = flags->GetEmpty(false);
  mojo_flags->present = flags->GetPresent(false);
  mojo_flags->exclusive = flags->GetExclusive(false);
  mojo_flags->inuse = flags->GetInuse(false);
  mojo_flags->mute = flags->GetMute(false);
  mojo_flags->unpowered = flags->GetUnpowered(false);

  return mojo_flags;
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

// Convert Protocol ReaderStateOut to Mojo SmartCardReaderStateOut.
device::mojom::SmartCardReaderStateOutPtr ToMojoReaderStateOut(
    protocol::SmartCardEmulation::ReaderStateOut& proto_state) {
  auto mojo_state = device::mojom::SmartCardReaderStateOut::New();

  mojo_state->reader = proto_state.GetReader();
  mojo_state->event_count = proto_state.GetEventCount();
  mojo_state->answer_to_reset = ToVector(proto_state.GetAtr());
  mojo_state->event_state = ToMojoReaderStateFlags(proto_state.GetEventState());
  return mojo_state;
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

device::mojom::SmartCardProtocol ToMojoSmartCardProtocol(
    const std::optional<std::string>& protocol_str) {
  if (!protocol_str.has_value() || protocol_str->empty()) {
    return device::mojom::SmartCardProtocol::kUndefined;
  }

  namespace Protocol = protocol::SmartCardEmulation::ProtocolEnum;
  using MojomProtocol = device::mojom::SmartCardProtocol;

  static const base::NoDestructor<base::flat_map<std::string, MojomProtocol>>
      kProtocolMap({
          {Protocol::T0, MojomProtocol::kT0},
          {Protocol::T1, MojomProtocol::kT1},
          {Protocol::Raw, MojomProtocol::kRaw},
      });

  auto it = kProtocolMap->find(*protocol_str);
  if (it != kProtocolMap->end()) {
    return it->second;
  }
  NOTREACHED();
}

// Convert optional Protocol array to standard vector required by Mojo.
std::vector<std::string> ToMojoStringVector(
    std::unique_ptr<protocol::Array<String>> in_readers) {
  if (!in_readers) {
    return {};
  }
  return std::move(*in_readers);
}

// Convert the Protocol reader states array to a vector of Mojo reader states.
std::vector<device::mojom::SmartCardReaderStateOutPtr> ToMojoReaderStates(
    std::unique_ptr<
        protocol::Array<protocol::SmartCardEmulation::ReaderStateOut>>
        in_states) {
  if (!in_states) {
    return {};
  }

  std::vector<device::mojom::SmartCardReaderStateOutPtr> out_states;
  out_states.reserve(in_states->size());

  for (const auto& state : *in_states) {
    out_states.push_back(ToMojoReaderStateOut(*state));
  }

  return out_states;
}

// Convert Protocol ConnectionState to Mojo SmartCardConnectionState.
device::mojom::SmartCardConnectionState ToMojoSmartCardConnectionState(
    const protocol::SmartCardEmulation::ConnectionState state) {
  namespace Protocol = protocol::SmartCardEmulation::ConnectionStateEnum;
  using MojomState = device::mojom::SmartCardConnectionState;

  static const base::NoDestructor<base::flat_map<std::string, MojomState>>
      kStateMap({
          {Protocol::Specific, MojomState::kSpecific},
          {Protocol::Negotiable, MojomState::kNegotiable},
          {Protocol::Powered, MojomState::kPowered},
          {Protocol::Swallowed, MojomState::kSwallowed},
          {Protocol::Present, MojomState::kPresent},
          {Protocol::Absent, MojomState::kAbsent},
      });

  auto it = kStateMap->find(state);
  if (it != kStateMap->end()) {
    return it->second;
  }

  // The DevTools dispatcher guarantees that 'state' matches one of the
  // string literals defined in the PDL (ConnectionStateEnum), so this branch
  // is unreachable unless the PDL and this map are out of sync.
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
  frontend_ =
      std::make_unique<SmartCardEmulation::Frontend>(dispatcher->channel());
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

SmartCardEmulationHandler::PendingConnect::PendingConnect(
    device::mojom::SmartCardContext::ConnectCallback callback,
    mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher> watcher)
    : PendingRequestImpl(std::move(callback)), watcher_(std::move(watcher)) {}

SmartCardEmulationHandler::PendingConnect::~PendingConnect() = default;
SmartCardEmulationHandler::PendingConnect::PendingConnect(PendingConnect&&) =
    default;

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

base::expected<void, std::string>
SmartCardEmulationHandler::CompleteListReaders(
    const std::string& request_id,
    std::vector<std::string> readers) {
  ASSIGN_OR_RETURN(auto req,
                   TakePendingRequest<PendingListReaders>(request_id));
  std::move(req.callback())
      .Run(device::mojom::SmartCardListReadersResult::NewReaders(
          std::move(readers)));
  return base::ok();
}

base::expected<void, std::string>
SmartCardEmulationHandler::CompleteEstablishContext(
    const std::string& request_id,
    const uint32_t context_id) {
  ASSIGN_OR_RETURN(auto req,
                   TakePendingRequest<PendingCreateContext>(request_id));

  auto context_impl = std::make_unique<EmulatedSmartCardContext>(
      weak_ptr_factory_.GetWeakPtr(), context_id);

  mojo::PendingRemote<device::mojom::SmartCardContext> remote;

  mojo::MakeSelfOwnedReceiver(std::move(context_impl),
                              remote.InitWithNewPipeAndPassReceiver());

  std::move(req.callback())
      .Run(device::mojom::SmartCardCreateContextResult::NewContext(
          std::move(remote)));
  return base::ok();
}

base::expected<void, std::string> SmartCardEmulationHandler::CompleteConnect(
    const std::string& request_id,
    const uint32_t handle,
    device::mojom::SmartCardProtocol active_protocol) {
  ASSIGN_OR_RETURN(auto req, TakePendingRequest<PendingConnect>(request_id));

  auto success_data = device::mojom::SmartCardConnectSuccess::New();
  success_data->active_protocol = active_protocol;

  mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher> watcher =
      req.TakeWatcher();

  auto connection_impl = std::make_unique<EmulatedSmartCardConnection>(
      weak_ptr_factory_.GetWeakPtr(), handle, std::move(watcher));

  mojo::PendingRemote<device::mojom::SmartCardConnection> connection_remote;

  mojo::MakeSelfOwnedReceiver(
      std::move(connection_impl),
      connection_remote.InitWithNewPipeAndPassReceiver());

  success_data->connection = std::move(connection_remote);

  std::move(req.callback())
      .Run(device::mojom::SmartCardConnectResult::NewSuccess(
          std::move(success_data)));

  return base::ok();
}

base::expected<void, std::string>
SmartCardEmulationHandler::CompleteGetStatusChange(
    const std::string& request_id,
    std::vector<device::mojom::SmartCardReaderStateOutPtr> reader_states) {
  ASSIGN_OR_RETURN(auto req,
                   TakePendingRequest<PendingGetStatusChange>(request_id));
  std::move(req.callback())
      .Run(device::mojom::SmartCardStatusChangeResult::NewReaderStates(
          std::move(reader_states)));
  return base::ok();
}

base::expected<void, std::string> SmartCardEmulationHandler::CompleteStatus(
    const std::string& request_id,
    device::mojom::SmartCardStatusPtr status_data) {
  ASSIGN_OR_RETURN(auto req, TakePendingRequest<PendingStatus>(request_id));
  std::move(req.callback())
      .Run(device::mojom::SmartCardStatusResult::NewStatus(
          std::move(status_data)));
  return base::ok();
}

base::expected<void, std::string> SmartCardEmulationHandler::CompleteDataResult(
    const std::string& request_id,
    std::vector<uint8_t> response) {
  ASSIGN_OR_RETURN(auto req, TakePendingRequest<PendingDataResult>(request_id));
  std::move(req.callback())
      .Run(device::mojom::SmartCardDataResult::NewData(std::move(response)));
  return base::ok();
}

base::expected<void, std::string>
SmartCardEmulationHandler::CompleteBeginTransaction(
    const std::string& request_id,
    uint32_t handle) {
  ASSIGN_OR_RETURN(auto req,
                   TakePendingRequest<PendingBeginTransaction>(request_id));

  mojo::PendingAssociatedRemote<device::mojom::SmartCardTransaction>
      transaction_remote;

  auto transaction_impl = std::make_unique<EmulatedSmartCardTransaction>(
      weak_ptr_factory_.GetWeakPtr(), handle);

  mojo::MakeSelfOwnedAssociatedReceiver(
      std::move(transaction_impl),
      transaction_remote.InitWithNewEndpointAndPassReceiver());

  std::move(req.callback())
      .Run(device::mojom::SmartCardTransactionResult::NewTransaction(
          std::move(transaction_remote)));
  return base::ok();
}

base::expected<void, std::string>
SmartCardEmulationHandler::CompletePlainResult(const std::string& request_id) {
  ASSIGN_OR_RETURN(auto req,
                   TakePendingRequest<PendingPlainResult>(request_id));
  std::move(req.callback())
      .Run(device::mojom::SmartCardResult::NewSuccess(
          device::mojom::SmartCardSuccess::kOk));
  return base::ok();
}

base::expected<void, std::string>
SmartCardEmulationHandler::CompleteReleaseContext(
    const std::string& request_id) {
  RETURN_IF_ERROR(TakePendingRequest<PendingReleaseContext>(request_id));
  return base::ok();
}

base::expected<void, std::string> SmartCardEmulationHandler::FailRequest(
    const std::string& request_id,
    device::mojom::SmartCardError error) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    return base::unexpected("Request ID not found");
  }

  PendingRequest request_variant = std::move(it->second);
  pending_requests_.erase(it);

  std::visit([error](auto&& request) { request.ReportError(error); },
             request_variant);

  return base::ok();
}

DispatchResponse SmartCardEmulationHandler::ReportEstablishContextResult(
    const String& in_requestId,
    const int in_contextId) {
  RETURN_IF_ERROR(CompleteEstablishContext(in_requestId, in_contextId),
                  [](const std::string& error) {
                    return DispatchResponse::ServerError(error);
                  });
  return DispatchResponse::Success();
}

DispatchResponse SmartCardEmulationHandler::ReportListReadersResult(
    const String& in_requestId,
    std::unique_ptr<protocol::Array<String>> in_readers) {
  RETURN_IF_ERROR(CompleteListReaders(
                      in_requestId, ToMojoStringVector(std::move(in_readers))),
                  [](const std::string& error) {
                    return DispatchResponse::ServerError(error);
                  });

  return DispatchResponse::Success();
}

DispatchResponse SmartCardEmulationHandler::ReportConnectResult(
    const String& in_requestId,
    const int in_handle,
    std::optional<String> in_activeProtocol) {
  RETURN_IF_ERROR(CompleteConnect(in_requestId, in_handle,
                                  ToMojoSmartCardProtocol(in_activeProtocol)),
                  [](const std::string& error) {
                    return DispatchResponse::ServerError(error);
                  });
  return DispatchResponse::Success();
}

DispatchResponse SmartCardEmulationHandler::ReportGetStatusChangeResult(
    const String& in_requestId,
    std::unique_ptr<
        protocol::Array<protocol::SmartCardEmulation::ReaderStateOut>>
        in_readerStates) {
  RETURN_IF_ERROR(
      CompleteGetStatusChange(in_requestId,
                              ToMojoReaderStates(std::move(in_readerStates))),
      [](const std::string& error) {
        return DispatchResponse::ServerError(error);
      });

  return DispatchResponse::Success();
}

DispatchResponse SmartCardEmulationHandler::ReportBeginTransactionResult(
    const String& in_requestId,
    const int in_handle) {
  RETURN_IF_ERROR(CompleteBeginTransaction(in_requestId, in_handle),
                  [](const std::string& error) {
                    return DispatchResponse::ServerError(error);
                  });

  return DispatchResponse::Success();
}

DispatchResponse SmartCardEmulationHandler::ReportDataResult(
    const String& in_requestId,
    const Binary& in_data) {
  RETURN_IF_ERROR(
      CompleteDataResult(in_requestId,
                         std::vector<uint8_t>(in_data.begin(), in_data.end())),
      [](const std::string& error) {
        return DispatchResponse::ServerError(error);
      });

  return DispatchResponse::Success();
}

DispatchResponse SmartCardEmulationHandler::ReportPlainResult(
    const String& in_requestId) {
  RETURN_IF_ERROR(CompletePlainResult(in_requestId),
                  [](const std::string& error) {
                    return DispatchResponse::ServerError(error);
                  });

  return DispatchResponse::Success();
}

DispatchResponse SmartCardEmulationHandler::ReportStatusResult(
    const String& in_requestId,
    const String& in_readerName,
    const String& in_state,
    const Binary& in_atr,
    std::optional<String> in_protocol) {
  std::vector<uint8_t> atr(in_atr.begin(), in_atr.end());

  RETURN_IF_ERROR(
      CompleteStatus(
          in_requestId,
          device::mojom::SmartCardStatus::New(
              in_readerName, ToMojoSmartCardConnectionState(in_state),
              ToMojoSmartCardProtocol(in_protocol), std::move(atr))),
      [](const std::string& error) {
        return DispatchResponse::ServerError(error);
      });

  return DispatchResponse::Success();
}

DispatchResponse SmartCardEmulationHandler::ReportReleaseContextResult(
    const String& in_requestId) {
  RETURN_IF_ERROR(CompleteReleaseContext(in_requestId),
                  [](const std::string& error) {
                    return DispatchResponse::ServerError(error);
                  });
  return DispatchResponse::Success();
}

DispatchResponse SmartCardEmulationHandler::ReportError(
    const String& in_requestId,
    const String& in_resultCode) {
  RETURN_IF_ERROR(
      FailRequest(in_requestId, ToMojoSmartCardError(in_resultCode)),
      [](const std::string& error) {
        return DispatchResponse::ServerError(error);
      });
  return DispatchResponse::Success();
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

  AddPendingRequest(request_id,
                    PendingConnect(std::move(callback), std::move(watcher)));

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

void SmartCardEmulationHandler::OnReleaseContext(uint32_t context_id) {
  std::string request_id = GenerateRequestId();
  AddPendingRequest(request_id, PendingReleaseContext());
  frontend_->ReleaseContextRequested(request_id, context_id);
}

}  // namespace content::protocol
