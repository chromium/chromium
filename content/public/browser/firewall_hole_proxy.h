// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FIREWALL_HOLE_PROXY_H_
#define CONTENT_PUBLIC_BROWSER_FIREWALL_HOLE_PROXY_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"

namespace content {

// FirewallHoleProxy represents a hole in the ChromeOS system firewall.
// When this class gets destroyed, it's supposed the close the corresponding
// firewall hole.
class CONTENT_EXPORT FirewallHoleProxy {
 public:
  using OpenCallback =
      base::OnceCallback<void(std::unique_ptr<FirewallHoleProxy>)>;

  virtual ~FirewallHoleProxy() = default;
};

// Opens a TCP port on the system firewall for the given |interface|
// (or all interfaces if |interface| is ""). On success invokes the callback
// with a valid FirewallHoleProxy; on failure supplies nullptr.
CONTENT_EXPORT void OpenTCPFirewallHole(const std::string& interface,
                         uint16_t port,
                         FirewallHoleProxy::OpenCallback callback);

// Opens a UDP port on the system firewall for the given |interface|
// (or all interfaces if |interface| is ""). On success invokes the callback
// with a valid FirewallHoleProxy; on failure supplies nullptr.
CONTENT_EXPORT void OpenUDPFirewallHole(const std::string& interface,
                         uint16_t port,
                         FirewallHoleProxy::OpenCallback callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FIREWALL_HOLE_PROXY_H_
