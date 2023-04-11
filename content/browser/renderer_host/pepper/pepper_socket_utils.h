// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/socket_permission_request.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ppapi/c/pp_stdint.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/firewall_hole/firewall_hole.h"
#include "net/base/ip_endpoint.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

struct PP_NetAddress_Private;

namespace content::pepper_socket_utils {

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

#if BUILDFLAG(IS_CHROMEOS)

void OpenTCPFirewallHole(const net::IPEndPoint& address,
                         chromeos::FirewallHole::OpenCallback callback);

void OpenUDPFirewallHole(const net::IPEndPoint& address,
                         chromeos::FirewallHole::OpenCallback callback);

#endif  // BUILDFLAG(IS_CHROMEOS)

// Annotations for TCP and UDP network requests. Defined here to make it easier
// to keep them in sync.
net::MutableNetworkTrafficAnnotationTag PepperTCPNetworkAnnotationTag();
net::MutableNetworkTrafficAnnotationTag PepperUDPNetworkAnnotationTag();

}  // namespace content::pepper_socket_utils

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_
