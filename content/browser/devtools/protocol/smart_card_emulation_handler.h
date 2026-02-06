// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SMART_CARD_EMULATION_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SMART_CARD_EMULATION_HANDLER_H_

#include <memory>

#include "base/types/expected.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/smart_card_emulation.h"
#include "content/browser/smart_card/emulation/emulated_smart_card_context_factory.h"
#include "content/browser/smart_card/emulation/smart_card_emulation_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/device/public/mojom/smart_card.mojom.h"

namespace content {
class RenderFrameHostImpl;
class WebContents;

namespace protocol {

// This class acts as the "Browser Side" implementation of PC/SC for DevTools.
// It intercepts Mojo calls from the Renderer and translates them into
// DevTools Protocol events.
class CONTENT_EXPORT SmartCardEmulationHandler
    : public DevToolsDomainHandler,
      public SmartCardEmulation::Backend,
      public content::WebContentsObserver,
      public SmartCardEmulationManager {
 public:
  // Requires WebContents to observe navigations.
  explicit SmartCardEmulationHandler(content::WebContents* web_contents);
  ~SmartCardEmulationHandler() override;

  // SmartCardEmulation::Backend implementation.
  DispatchResponse Enable() override;
  DispatchResponse Disable() override;

  template <typename CallbackType, typename ResultType>
  class PendingRequestImpl {
   public:
    explicit PendingRequestImpl(CallbackType cb) : callback_(std::move(cb)) {}

    PendingRequestImpl(PendingRequestImpl&&) = default;
    PendingRequestImpl& operator=(PendingRequestImpl&&) = default;

    virtual ~PendingRequestImpl() = default;

    void ReportError(device::mojom::SmartCardError error) {
      std::move(callback_).Run(ResultType::NewError(error));
    }

    CallbackType& callback() { return callback_; }

   private:
    CallbackType callback_;
  };

  using PendingCreateContext = PendingRequestImpl<
      device::mojom::SmartCardContextFactory::CreateContextCallback,
      device::mojom::SmartCardCreateContextResult>;

  using PendingListReaders =
      PendingRequestImpl<device::mojom::SmartCardContext::ListReadersCallback,
                         device::mojom::SmartCardListReadersResult>;

  using PendingGetStatusChange = PendingRequestImpl<
      device::mojom::SmartCardContext::GetStatusChangeCallback,
      device::mojom::SmartCardStatusChangeResult>;

  class PendingConnect : public PendingRequestImpl<
                             device::mojom::SmartCardContext::ConnectCallback,
                             device::mojom::SmartCardConnectResult> {
   public:
    PendingConnect(
        device::mojom::SmartCardContext::ConnectCallback callback,
        mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher> watcher);
    ~PendingConnect() override;
    PendingConnect(PendingConnect&&);

    mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher>
    TakeWatcher() {
      return std::move(watcher_);
    }

   private:
    mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher> watcher_;
  };

  // Handles Cancel, Disconnect, SetAttrib, EndTransaction.
  using PendingPlainResult = PendingRequestImpl<
      base::OnceCallback<void(device::mojom::SmartCardResultPtr)>,
      device::mojom::SmartCardResult>;

  // Handles Transmit, Control, GetAttrib.
  using PendingDataResult = PendingRequestImpl<
      base::OnceCallback<void(device::mojom::SmartCardDataResultPtr)>,
      device::mojom::SmartCardDataResult>;

  using PendingStatus =
      PendingRequestImpl<device::mojom::SmartCardConnection::StatusCallback,
                         device::mojom::SmartCardStatusResult>;

  using PendingBeginTransaction = PendingRequestImpl<
      device::mojom::SmartCardConnection::BeginTransactionCallback,
      device::mojom::SmartCardTransactionResult>;

  struct PendingReleaseContext {
    void ReportError(device::mojom::SmartCardError error) {}
  };

  using PendingRequest = std::variant<PendingCreateContext,
                                      PendingListReaders,
                                      PendingGetStatusChange,
                                      PendingConnect,
                                      PendingPlainResult,
                                      PendingDataResult,
                                      PendingStatus,
                                      PendingBeginTransaction,
                                      PendingReleaseContext>;

 private:
  static std::vector<SmartCardEmulationHandler*> ForAgentHost(
      DevToolsAgentHostImpl* host);

  // DevToolsDomainHandler implementation.
  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;

  // WebContentsObserver implementation.
  void ReadyToCommitNavigation(content::NavigationHandle* handle) override;

  // Registers the emulated Smart Card factory with the system delegate for
  // the given frame.
  void UpdateEmulationOverride(content::RenderFrameHost* rfh);

  DispatchResponse ReportEstablishContextResult(
      const String& in_requestId,
      const int in_contextId) override;

  DispatchResponse ReportListReadersResult(
      const String& in_requestId,
      std::unique_ptr<protocol::Array<String>> in_readers) override;

  DispatchResponse ReportConnectResult(
      const String& in_requestId,
      const int in_handle,
      std::optional<String> in_activeProtocol) override;

  DispatchResponse ReportGetStatusChangeResult(
      const String& in_requestId,
      std::unique_ptr<
          protocol::Array<protocol::SmartCardEmulation::ReaderStateOut>>
          in_readerStates) override;

  DispatchResponse ReportBeginTransactionResult(const String& in_requestId,
                                                const int in_handle) override;

  DispatchResponse ReportDataResult(const String& in_requestId,
                                    const Binary& in_data) override;

  DispatchResponse ReportPlainResult(const String& in_requestId) override;

  DispatchResponse ReportStatusResult(
      const String& in_requestId,
      const String& in_readerName,
      const String& in_state,
      const Binary& in_atr,
      std::optional<String> in_protocol) override;

  DispatchResponse ReportReleaseContextResult(
      const String& in_requestId) override;

  DispatchResponse ReportError(const String& in_requestId,
                               const String& in_resultCode) override;

  void OnCreateContext(
      device::mojom::SmartCardContextFactory::CreateContextCallback callback)
      override;

  void OnListReaders(
      uint32_t context_id,
      device::mojom::SmartCardContext::ListReadersCallback callback) override;

  void OnGetStatusChange(
      uint32_t context_id,
      base::TimeDelta timeout,
      std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states,
      device::mojom::SmartCardContext::GetStatusChangeCallback callback)
      override;

  void OnCancel(
      uint32_t context_id,
      device::mojom::SmartCardContext::CancelCallback callback) override;

  void OnConnect(
      uint32_t context_id,
      const std::string& reader,
      device::mojom::SmartCardShareMode share_mode,
      device::mojom::SmartCardProtocolsPtr preferred_protocols,
      mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher> watcher,
      device::mojom::SmartCardContext::ConnectCallback callback) override;

  void OnDisconnect(
      uint32_t handle,
      device::mojom::SmartCardDisposition disposition,
      device::mojom::SmartCardConnection::DisconnectCallback callback) override;

  void OnControl(
      uint32_t handle,
      uint32_t control_code,
      const std::vector<uint8_t>& data,
      device::mojom::SmartCardConnection::ControlCallback callback) override;

  void OnGetAttrib(
      uint32_t handle,
      uint32_t id,
      device::mojom::SmartCardConnection::GetAttribCallback callback) override;

  void OnTransmit(
      uint32_t handle,
      device::mojom::SmartCardProtocol protocol,
      const std::vector<uint8_t>& data,
      device::mojom::SmartCardConnection::TransmitCallback callback) override;

  void OnStatus(
      uint32_t handle,
      device::mojom::SmartCardConnection::StatusCallback callback) override;

  void OnSetAttrib(
      uint32_t handle,
      uint32_t id,
      const std::vector<uint8_t>& data,
      device::mojom::SmartCardConnection::SetAttribCallback callback) override;

  void OnBeginTransaction(
      uint32_t handle,
      device::mojom::SmartCardConnection::BeginTransactionCallback callback)
      override;

  void OnEndTransaction(
      uint32_t handle,
      device::mojom::SmartCardDisposition disposition,
      device::mojom::SmartCardTransaction::EndTransactionCallback callback)
      override;

  void OnReleaseContext(uint32_t context_id) override;

  void AddPendingRequest(const std::string& request_id, PendingRequest request);

  template <typename T>
  base::expected<T, std::string> TakePendingRequest(
      const std::string& request_id);

  // The Handler needs to store Mojo callbacks for "paused" requests so they can
  // be resumed later when the DevTools frontend replies.
  std::unordered_map<std::string, PendingRequest> pending_requests_;

  bool IsEnabled() const { return !!factory_; }

  base::expected<void, std::string> CompleteEstablishContext(
      const std::string& request_id,
      const uint32_t context_id);

  base::expected<void, std::string> CompleteListReaders(
      const std::string& request_id,
      std::vector<std::string> readers);

  base::expected<void, std::string> CompleteConnect(
      const std::string& request_id,
      const uint32_t handle,
      device::mojom::SmartCardProtocol active_protocol);

  base::expected<void, std::string> CompleteGetStatusChange(
      const std::string& in_requestId,
      std::vector<device::mojom::SmartCardReaderStateOutPtr> reader_states);

  base::expected<void, std::string> CompleteStatus(
      const std::string& request_id,
      device::mojom::SmartCardStatusPtr status_data);

  base::expected<void, std::string> CompleteDataResult(
      const std::string& request_id,
      std::vector<uint8_t> response);

  base::expected<void, std::string> CompleteBeginTransaction(
      const std::string& request_id,
      uint32_t handle);

  base::expected<void, std::string> CompletePlainResult(
      const std::string& request_id);

  base::expected<void, std::string> CompleteReleaseContext(
      const std::string& request_id);

  base::expected<void, std::string> FailRequest(
      const std::string& request_id,
      device::mojom::SmartCardError error);

  // The frontend interface to send events to the DevTools client.
  std::unique_ptr<SmartCardEmulation::Frontend> frontend_;

  // The Factory that handles the actual Mojo emulation logic.
  std::unique_ptr<EmulatedSmartCardContextFactory> factory_;

  base::WeakPtrFactory<SmartCardEmulationHandler> weak_ptr_factory_{this};
};

}  // namespace protocol

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SMART_CARD_EMULATION_HANDLER_H_
