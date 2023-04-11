// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_socket_utils.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"

namespace content::pepper_socket_utils {

SocketPermissionRequest CreateSocketPermissionRequest(
    SocketPermissionRequest::OperationType type,
    const PP_NetAddress_Private& net_addr) {
  std::string host =
      ppapi::NetAddressPrivateImpl::DescribeNetAddress(net_addr, false);
  uint16_t port = 0;
  net::IPAddressBytes address;
  ppapi::NetAddressPrivateImpl::NetAddressToIPEndPoint(net_addr, &address,
                                                       &port);
  return SocketPermissionRequest(type, host, port);
}

bool CanUseSocketAPIs(bool external_plugin,
                      bool private_api,
                      const SocketPermissionRequest* params,
                      int render_process_id,
                      int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!external_plugin) {
    // Always allow socket APIs for out-process plugins (other than external
    // plugins instantiated by the embedder through
    // BrowserPpapiHost::CreateExternalPluginProcess).
    return true;
  }

  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host) {
    return false;
  }
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  if (!site_instance) {
    return false;
  }
  if (!GetContentClient()->browser()->AllowPepperSocketAPI(
          site_instance->GetBrowserContext(), site_instance->GetSiteURL(),
          private_api, params)) {
    LOG(ERROR) << "Host " << site_instance->GetSiteURL().host()
               << " cannot use socket API or destination is not allowed";
    return false;
  }

  return true;
}

#if BUILDFLAG(IS_CHROMEOS)
namespace {

// The entire IPv4 subnet 127.0.0.0/8 is for loopback. See RFC3330.
const uint8_t kIPv4LocalhostPrefix[] = {127};

bool IsLoopbackAddress(const net::IPAddress& address) {
  if (address.IsIPv4()) {
    return net::IPAddressStartsWith(address, kIPv4LocalhostPrefix);
  } else if (address.IsIPv6()) {
    // ::1 is the only loopback address in ipv6.
    return address == net::IPAddress::IPv6Localhost();
  }
  return false;
}

}  // namespace

void OpenTCPFirewallHole(const net::IPEndPoint& address,
                         chromeos::FirewallHole::OpenCallback callback) {
  if (IsLoopbackAddress(address.address())) {
    std::move(callback).Run(nullptr);
    return;
  }
  chromeos::FirewallHole::Open(chromeos::FirewallHole::PortType::kTcp,
                               address.port(), "" /*all interfaces*/,
                               std::move(callback));
}

void OpenUDPFirewallHole(const net::IPEndPoint& address,
                         chromeos::FirewallHole::OpenCallback callback) {
  if (IsLoopbackAddress(address.address())) {
    std::move(callback).Run(nullptr);
    return;
  }
  chromeos::FirewallHole::Open(chromeos::FirewallHole::PortType::kUdp,
                               address.port(), "" /*all interfaces*/,
                               std::move(callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

net::MutableNetworkTrafficAnnotationTag PepperTCPNetworkAnnotationTag() {
  return net::MutableNetworkTrafficAnnotationTag(
      net::DefineNetworkTrafficAnnotation("pepper_tcp_socket",
                                          R"(
        semantics {
          sender: "Pepper TCP Socket"
          description:
            "Pepper plugins use this API to send and receive data over the "
            "network using TCP connections. This inteface is used by Flash and "
            "PDF viewer, and Chrome Apps which use plugins to send/receive TCP "
            "traffic (require Chrome Apps TCP socket permission). This "
            "interface allows creation of client and server sockets."
          trigger:
            "A request from a Pepper plugin."
          data: "Any data that the plugin sends."
          destination: OTHER
          destination_other:
            "Data can be sent to any destination."
        }
        policy {
          cookies_allowed: NO
          setting:
            "These requests cannot be disabled, but will not happen if user "
            "does not use Flash, internal PDF Viewer, or Chrome Apps that use "
            "Pepper interface."
          chrome_policy {
            DefaultPluginsSetting {
              DefaultPluginsSetting: 2
            }
          }
          chrome_policy {
            AlwaysOpenPdfExternally {
              AlwaysOpenPdfExternally: true
            }
          }
          chrome_policy {
            ExtensionInstallBlocklist {
              ExtensionInstallBlocklist: {
                entries: '*'
              }
            }
          }
        })"));
}

net::MutableNetworkTrafficAnnotationTag PepperUDPNetworkAnnotationTag() {
  return net::MutableNetworkTrafficAnnotationTag(
      net::DefineNetworkTrafficAnnotation("pepper_udp_socket",
                                          R"(
        semantics {
          sender: "Pepper UDP Socket"
          description:
            "Pepper plugins use this API to send and receive data over the "
            "network using UDP connections. This inteface is used by Flash and "
            "PDF viewer, and Chrome Apps which use plugins to send/receive UDP "
            "traffic (require Chrome Apps UDP socket permission)."
          trigger:
            "A request from a Pepper plugin."
          data: "Any data that the plugin sends."
          destination: OTHER
          destination_other:
            "Data can be sent to any destination."
        }
        policy {
          cookies_allowed: NO
          setting:
            "These requests cannot be disabled, but will not happen if user "
            "does not use Flash, internal PDF Viewer, or Chrome Apps that use "
            "Pepper interface."
          chrome_policy {
            DefaultPluginsSetting {
              DefaultPluginsSetting: 2
            }
          }
          chrome_policy {
            AlwaysOpenPdfExternally {
              AlwaysOpenPdfExternally: true
            }
          }
          chrome_policy {
            ExtensionInstallBlocklist {
              ExtensionInstallBlocklist: {
                entries: '*'
              }
            }
          }
        })"));
}

}  // namespace content::pepper_socket_utils
