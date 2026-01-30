// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SMART_CARD_EMULATION_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SMART_CARD_EMULATION_HANDLER_H_

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

  bool IsEnabled() const { return !!factory_; }

  // The frontend interface to send events to the DevTools client.
  raw_ptr<SmartCardEmulation::Frontend> frontend_;

  // The Factory that handles the actual Mojo emulation logic.
  std::unique_ptr<EmulatedSmartCardContextFactory> factory_;
};

}  // namespace protocol

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SMART_CARD_EMULATION_HANDLER_H_
