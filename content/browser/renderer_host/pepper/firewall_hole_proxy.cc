// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/firewall_hole_proxy.h"

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/network/firewall_hole.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/firewall_hole.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace content {
namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
crosapi::mojom::FirewallHoleService* GetFirewallHoleService() {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::FirewallHoleService>()) {
    LOG(ERROR) << "FirewallHoleService is not available in Lacros";
    return nullptr;
  }
  return service->GetRemote<crosapi::mojom::FirewallHoleService>().get();
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)

class FirewallHoleProxyAsh : public content::FirewallHoleProxy {
 public:
  ~FirewallHoleProxyAsh() override = default;

  static std::unique_ptr<FirewallHoleProxyAsh> Create(
      std::unique_ptr<ash::FirewallHole> firewall_hole) {
    if (!firewall_hole) {
      return nullptr;
    }
    return base::WrapUnique(new FirewallHoleProxyAsh(std::move(firewall_hole)));
  }

 private:
  explicit FirewallHoleProxyAsh(
      std::unique_ptr<ash::FirewallHole> firewall_hole)
      : firewall_hole_(std::move(firewall_hole)) {}

  std::unique_ptr<ash::FirewallHole> firewall_hole_;
};

#else

class FirewallHoleProxyLacros : public content::FirewallHoleProxy {
 public:
  ~FirewallHoleProxyLacros() override = default;

  static std::unique_ptr<FirewallHoleProxyLacros> Create(
      mojo::PendingRemote<crosapi::mojom::FirewallHole> firewall_hole) {
    if (!firewall_hole) {
      return nullptr;
    }
    return base::WrapUnique(
        new FirewallHoleProxyLacros(std::move(firewall_hole)));
  }

 private:
  FirewallHoleProxyLacros(
      mojo::PendingRemote<crosapi::mojom::FirewallHole> firewall_hole)
      : firewall_hole_(std::move(firewall_hole)) {}

  mojo::Remote<crosapi::mojom::FirewallHole> firewall_hole_;
};

#endif

}  // namespace

void OpenTCPFirewallHole(const std::string& interface,
                         uint16_t port,
                         FirewallHoleProxy::OpenCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::FirewallHole::Open(
      ash::FirewallHole::PortType::TCP, port, interface,
      base::BindOnce(&FirewallHoleProxyAsh::Create).Then(std::move(callback)));
#else
  auto* firewall_hole_service = GetFirewallHoleService();
  if (!firewall_hole_service) {
    std::move(callback).Run(nullptr);
    return;
  }
  firewall_hole_service->OpenTCPFirewallHole(
      interface, port,
      base::BindOnce(&FirewallHoleProxyLacros::Create)
          .Then(std::move(callback)));
#endif
}

void OpenUDPFirewallHole(const std::string& interface,
                         uint16_t port,
                         FirewallHoleProxy::OpenCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::FirewallHole::Open(
      ash::FirewallHole::PortType::UDP, port, interface,
      base::BindOnce(&FirewallHoleProxyAsh::Create).Then(std::move(callback)));
#else
  auto* firewall_hole_service = GetFirewallHoleService();
  if (!firewall_hole_service) {
    std::move(callback).Run(nullptr);
    return;
  }
  firewall_hole_service->OpenUDPFirewallHole(
      interface, port,
      base::BindOnce(&FirewallHoleProxyLacros::Create)
          .Then(std::move(callback)));
#endif
}

}  // namespace content
