// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_UDP_SOCKET_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_UDP_SOCKET_MESSAGE_FILTER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_export.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_udp_socket.h"
#include "ppapi/host/resource_message_filter.h"
#include "services/network/public/mojom/udp_socket.mojom.h"

struct PP_NetAddress_Private;

namespace ppapi {

class SocketOptionData;

namespace host {
struct ReplyMessageContext;
}
}  // namespace ppapi

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace chromeos {
class FirewallHole;
}  // namespace chromeos

namespace content {

class BrowserPpapiHostImpl;

class CONTENT_EXPORT PepperUDPSocketMessageFilter
    : public ppapi::host::ResourceMessageFilter,
      public network::mojom::UDPSocketListener {
 public:
  PepperUDPSocketMessageFilter(BrowserPpapiHostImpl* host,
                               PP_Instance instance,
                               bool private_api);

  PepperUDPSocketMessageFilter(const PepperUDPSocketMessageFilter&) = delete;
  PepperUDPSocketMessageFilter& operator=(const PepperUDPSocketMessageFilter&) =
      delete;

  using CreateUDPSocketCallback = base::RepeatingCallback<void(
      network::mojom::NetworkContext* network_context,
      mojo::PendingReceiver<network::mojom::UDPSocket> socket_receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> socket_listener)>;

  static void SetCreateUDPSocketCallbackForTesting(
      const CreateUDPSocketCallback* create_udp_socket_callback);

  static size_t GetNumInstances();

 protected:
  ~PepperUDPSocketMessageFilter() override;

 private:
  enum SocketOption {
    SOCKET_OPTION_ADDRESS_REUSE = 1 << 0,
    SOCKET_OPTION_BROADCAST = 1 << 1,
    SOCKET_OPTION_RCVBUF_SIZE = 1 << 2,
    SOCKET_OPTION_SNDBUF_SIZE = 1 << 3,
    SOCKET_OPTION_MULTICAST_LOOP = 1 << 4,
    SOCKET_OPTION_MULTICAST_TTL = 1 << 5
  };

  struct PendingSend {
    PendingSend(const net::IPAddress& address,
                int port,
                std::vector<uint8_t> data,
                const ppapi::host::ReplyMessageContext& context);
    PendingSend(const PendingSend& other);
    ~PendingSend();

    net::IPAddress address;
    int port;
    std::vector<uint8_t> data;
    ppapi::host::ReplyMessageContext context;
  };

  // ppapi::host::ResourceMessageFilter overrides.
  void OnFilterDestroyed() override;
  scoped_refptr<base::SequencedTaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& message) override;
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  int32_t OnMsgSetOption(const ppapi::host::HostMessageContext* context,
                         PP_UDPSocket_Option name,
                         const ppapi::SocketOptionData& value);
  int32_t OnMsgBind(const ppapi::host::HostMessageContext* context,
                    const PP_NetAddress_Private& addr);
  int32_t OnMsgSendTo(const ppapi::host::HostMessageContext* context,
                      const std::string& data,
                      const PP_NetAddress_Private& addr);
  int32_t OnMsgClose(const ppapi::host::HostMessageContext* context);
  int32_t OnMsgRecvSlotAvailable(
      const ppapi::host::HostMessageContext* context);
  int32_t OnMsgJoinGroup(const ppapi::host::HostMessageContext* context,
                         const PP_NetAddress_Private& addr);
  int32_t OnMsgLeaveGroup(const ppapi::host::HostMessageContext* context,
                          const PP_NetAddress_Private& addr);

  void DoBindCallback(mojo::PendingReceiver<network::mojom::UDPSocketListener>
                          listener_receiver,
                      const ppapi::host::ReplyMessageContext& context,
                      int result,
                      const std::optional<net::IPEndPoint>& local_addr_out);
  void OnBindComplete(mojo::PendingReceiver<network::mojom::UDPSocketListener>
                          listener_receiver,
                      const ppapi::host::ReplyMessageContext& context,
                      const PP_NetAddress_Private& net_address);
#if BUILDFLAG(IS_CHROMEOS)
  void OnFirewallHoleOpened(
      mojo::PendingReceiver<network::mojom::UDPSocketListener>
          listener_receiver,
      const ppapi::host::ReplyMessageContext& context,
      const PP_NetAddress_Private& net_address,
      std::unique_ptr<chromeos::FirewallHole> hole);
#endif  // BUILDFLAG(IS_CHROMEOS)
  void StartPendingSend();
  void Close();

  // network::mojom::UDPSocketListener override:
  void OnReceived(int result,
                  const std::optional<net::IPEndPoint>& src_addr,
                  std::optional<base::span<const uint8_t>> data) override;

  void OnSendToCompleted(int net_result);
  void FinishPendingSend(int net_result);

  void SendBindReply(const ppapi::host::ReplyMessageContext& context,
                     int32_t result,
                     const PP_NetAddress_Private& addr);
  void SendRecvFromResult(int32_t result,
                          const std::string& data,
                          const PP_NetAddress_Private& addr);
  void SendRecvFromResultOnIOThread(int32_t result,
                                    const std::string& data,
                                    const PP_NetAddress_Private& addr);
  void SendSendToReply(const ppapi::host::ReplyMessageContext& context,
                       int32_t result,
                       int32_t bytes_written);

  void SendBindError(const ppapi::host::ReplyMessageContext& context,
                     int32_t result);
  void SendRecvFromError(int32_t result);
  void SendSendToError(const ppapi::host::ReplyMessageContext& context,
                       int32_t result);
  void PipeClosed();

  int32_t CanUseMulticastAPI(const PP_NetAddress_Private& addr);

  template <class ReturnMessage>
  base::OnceCallback<void(int result)> CreateCompletionCallback(
      const ppapi::host::HostMessageContext* context);

  template <class ReturnMessage>
  void ReturnResult(const ppapi::host::ReplyMessageContext& context,
                    std::unique_ptr<int> result);

  // Bitwise-or of SocketOption flags. This stores the state about whether
  // each option is set before Bind() is called.
  int socket_options_;

  // Locally cached value of buffer size.
  int32_t rcvbuf_size_;
  int32_t sndbuf_size_;

  // Multicast options, if socket hasn't been bound
  int multicast_ttl_;
  int32_t can_use_multicast_;

  bool closed_;

  base::queue<PendingSend> pending_sends_;

  size_t remaining_recv_slots_;

  bool external_plugin_;
  bool private_api_;

  int render_process_id_;
  int render_frame_id_;

  const bool is_potentially_secure_plugin_context_;

  // Bound (in a Mojo sense) when binding (in a network sense) starts. Closed in
  // Close() and on Mojo pipe errors. Must only be accessed (and destroyed) on
  // UI thread.
  mojo::Remote<network::mojom::UDPSocket> socket_;

  // Bound (in a Mojo sense) when binding (in a network sense) completes.
  // Binding late avoids receiving data when still setting up the socket. Closed
  // in Close() and on Mojo pipe errors. Must only be accessed (and destroyed)
  // on UI thread.
  mojo::Receiver<network::mojom::UDPSocketListener> receiver_{this};

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<chromeos::FirewallHole> firewall_hole_;
  // Allows for cancellation of opening a hole in the firewall in the case the
  // network service crashes.
  base::WeakPtrFactory<PepperUDPSocketMessageFilter>
      firewall_hole_weak_ptr_factory_{this};
#endif  // BUILDFLAG(IS_CHROMEOS)
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_UDP_SOCKET_MESSAGE_FILTER_H_
