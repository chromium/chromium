// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MESSAGE_SERVICE_API_H_
#define EXTENSIONS_BROWSER_MESSAGE_SERVICE_API_H_

#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/mojom/message_port.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace extensions {

// A public interface that the extension/browser code can depend on the
// MessageService without causing a dependency cycle.
class MessageServiceApi {
 public:
  virtual ~MessageServiceApi() = default;

  using ExternalConnectionInfo = mojom::ExternalConnectionInfo;
  using Source = absl::variant<content::RenderFrameHost*, WorkerId>;

  virtual void OpenChannelToExtension(
      content::BrowserContext* context,
      Source source,
      const PortId& source_port_id,
      const ExternalConnectionInfo& info,
      mojom::ChannelType channel_type,
      const std::string& channel_name,
      mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
      mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
          port_host) = 0;
  virtual void OpenChannelToNativeApp(
      content::BrowserContext* context,
      Source source,
      const PortId& source_port_id,
      const std::string& native_app_name,
      mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
      mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
          port_host) = 0;
  virtual void OpenChannelToTab(
      content::BrowserContext* context,
      Source source,
      const PortId& source_port_id,
      int tab_id,
      int frame_id,
      const std::string& document_id,
      mojom::ChannelType channel_type,
      const std::string& channel_name,
      mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
      mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
          port_host) = 0;

  static MessageServiceApi* GetMessageService();
  static void SetMessageService(MessageServiceApi* message_service);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MESSAGE_SERVICE_API_H_
