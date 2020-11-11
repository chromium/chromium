// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/socket_permission_request.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ppapi/c/pp_stdint.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/network/firewall_hole.h"
#include "net/base/ip_endpoint.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

struct PP_NetAddress_Private;

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace chromeos {
class FirewallHole;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace net {
class X509Certificate;
}

namespace ppapi {
class PPB_X509Certificate_Fields;
}

namespace content {

namespace pepper_socket_utils {

SocketPermissionRequest CreateSocketPermissionRequest(
    SocketPermissionRequest::OperationType type,
    const PP_NetAddress_Private& net_addr);

// Returns true if the socket operation specified by |params| is allowed.
// If |params| is NULL, this method checks the basic "socket" permission, which
// is for those operations that don't require a specific socket permission rule.
bool CanUseSocketAPIs(bool external_plugin,
                      bool private_api,
                      const SocketPermissionRequest* params,
                      int render_process_id,
                      int render_frame_id);

// Extracts the certificate field data from a net::X509Certificate into
// PPB_X509Certificate_Fields.
bool GetCertificateFields(const net::X509Certificate& cert,
                          ppapi::PPB_X509Certificate_Fields* fields);

// Extracts the certificate field data from the DER representation of a
// certificate into PPB_X509Certificate_Fields.
bool GetCertificateFields(const char* der,
                          uint32_t length,
                          ppapi::PPB_X509Certificate_Fields* fields);

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Returns true if the open operation is in progress.
void OpenTCPFirewallHole(const net::IPEndPoint& address,
                         chromeos::FirewallHole::OpenCallback callback);

void OpenUDPFirewallHole(const net::IPEndPoint& address,
                         chromeos::FirewallHole::OpenCallback callback);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Annotations for TCP and UDP network requests. Defined here to make it easier
// to keep them in sync.
net::MutableNetworkTrafficAnnotationTag PepperTCPNetworkAnnotationTag();
net::MutableNetworkTrafficAnnotationTag PepperUDPNetworkAnnotationTag();

}  // namespace pepper_socket_utils

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_
