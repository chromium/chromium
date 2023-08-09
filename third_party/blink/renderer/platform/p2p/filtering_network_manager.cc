// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/p2p/filtering_network_manager.h"

#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/media_permission.h"
#include "third_party/blink/renderer/platform/p2p/ipc_network_manager.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

FilteringNetworkManager::FilteringNetworkManager(
    IpcNetworkManager* network_manager,
    media::MediaPermission* media_permission,
    bool allow_mdns_obfuscation)
    : FilteringNetworkManager(network_manager->AsWeakPtrForSignalingThread(),
                              media_permission,
                              allow_mdns_obfuscation) {}

// DO NOT dereference/check `network_manager_for_signaling_thread` in the ctor!
// Doing so would bind its WeakFactory to the constructing thread (main thread)
// instead of the thread `this` lives in (signaling thread).
FilteringNetworkManager::FilteringNetworkManager(
    base::WeakPtr<rtc::NetworkManager> network_manager_for_signaling_thread,
    media::MediaPermission* media_permission,
    bool allow_mdns_obfuscation)
    : network_manager_for_signaling_thread_(
          std::move(network_manager_for_signaling_thread)),
      media_permission_(media_permission),
      allow_mdns_obfuscation_(allow_mdns_obfuscation) {
  DETACH_FROM_THREAD(thread_checker_);
  set_enumeration_permission(ENUMERATION_BLOCKED);

  // If the feature is not enabled, just return ALLOWED as it's requested.
  if (!media_permission_) {
    started_permission_check_ = true;
    set_enumeration_permission(ENUMERATION_ALLOWED);
    VLOG(3) << "media_permission is not passed, granting permission";
    return;
  }
}

FilteringNetworkManager::~FilteringNetworkManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

base::WeakPtr<FilteringNetworkManager> FilteringNetworkManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FilteringNetworkManager::Initialize() {
  rtc::NetworkManagerBase::Initialize();
  if (media_permission_)
    CheckPermission();
}

void FilteringNetworkManager::StartUpdating() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(started_permission_check_);
  DCHECK(network_manager_for_signaling_thread_);

  if (!start_updating_called_) {
    start_updating_called_ = true;
    network_manager_for_signaling_thread_->SignalNetworksChanged.connect(
        this, &FilteringNetworkManager::OnNetworksChanged);
  }

  // Update |pending_network_update_| and |start_count_| before calling
  // StartUpdating, in case the update signal is fired synchronously.
  pending_network_update_ = true;
  ++start_count_;
  network_manager_for_signaling_thread_->StartUpdating();
  // If we have not sent the first update, which implies we have not received
  // the first network update from the base network manager, we wait until the
  // base network manager signals a network change for us to populate the
  // network information in |OnNetworksChanged| and fire the event there.
  if (sent_first_update_) {
    FireEventIfStarted();
  }
}

void FilteringNetworkManager::StopUpdating() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (network_manager_for_signaling_thread_)
    network_manager_for_signaling_thread_->StopUpdating();
  DCHECK_GT(start_count_, 0);
  --start_count_;
}

std::vector<const rtc::Network*> FilteringNetworkManager::GetNetworks() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<const rtc::Network*> networks;

  if (enumeration_permission() == ENUMERATION_ALLOWED) {
    for (const rtc::Network* network : GetNetworksInternal()) {
      networks.push_back(const_cast<rtc::Network*>(network));
    }
  }

  VLOG(3) << "GetNetworks() returns " << networks.size() << " networks.";
  return networks;
}

webrtc::MdnsResponderInterface* FilteringNetworkManager::GetMdnsResponder()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!network_manager_for_signaling_thread_)
    return nullptr;

  // mDNS responder is set to null if we have the enumeration permission or the
  // mDNS obfuscation of IPs is disallowed.
  if (enumeration_permission() == ENUMERATION_ALLOWED ||
      !allow_mdns_obfuscation_) {
    return nullptr;
  }

  return network_manager_for_signaling_thread_->GetMdnsResponder();
}

void FilteringNetworkManager::CheckPermission() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!started_permission_check_);

  started_permission_check_ = true;
  pending_permission_checks_ = 2;

  VLOG(1) << "FilteringNetworkManager checking permission status.";
  // Request for media permission asynchronously.
  media_permission_->HasPermission(
      media::MediaPermission::Type::kAudioCapture,
      WTF::BindOnce(&FilteringNetworkManager::OnPermissionStatus,
                    GetWeakPtr()));
  media_permission_->HasPermission(
      media::MediaPermission::Type::kVideoCapture,
      WTF::BindOnce(&FilteringNetworkManager::OnPermissionStatus,
                    GetWeakPtr()));
}

void FilteringNetworkManager::OnPermissionStatus(bool granted) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(pending_permission_checks_, 0);
  VLOG(1) << "FilteringNetworkManager received permission status: "
          << (granted ? "granted" : "denied");
  blink::IPPermissionStatus old_status = GetIPPermissionStatus();

  --pending_permission_checks_;

  if (granted)
    set_enumeration_permission(ENUMERATION_ALLOWED);

  // If the IP permission status changed *and* we have an up-to-date network
  // list, fire a network change event.
  if (GetIPPermissionStatus() != old_status && !pending_network_update_)
    FireEventIfStarted();
}

void FilteringNetworkManager::OnNetworksChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(network_manager_for_signaling_thread_);

  pending_network_update_ = false;

  // Update the default local addresses.
  rtc::IPAddress ipv4_default;
  rtc::IPAddress ipv6_default;
  network_manager_for_signaling_thread_->GetDefaultLocalAddress(AF_INET,
                                                                &ipv4_default);
  network_manager_for_signaling_thread_->GetDefaultLocalAddress(AF_INET6,
                                                                &ipv6_default);
  set_default_local_addresses(ipv4_default, ipv6_default);

  // Copy and merge the networks. Fire a signal if the permission status is
  // known.
  std::vector<const rtc::Network*> networks =
      network_manager_for_signaling_thread_->GetNetworks();
  std::vector<std::unique_ptr<rtc::Network>> copied_networks;
  copied_networks.reserve(networks.size());
  for (const rtc::Network* network : networks) {
    auto copied_network = std::make_unique<rtc::Network>(*network);
    copied_network->set_default_local_address_provider(this);
    copied_network->set_mdns_responder_provider(this);
    copied_networks.push_back(std::move(copied_network));
  }
  bool changed;
  MergeNetworkList(std::move(copied_networks), &changed);
  // We wait until our permission status is known before firing a network
  // change signal, so that the listener(s) don't miss out on receiving a
  // full network list.
  if (changed && GetIPPermissionStatus() != blink::PERMISSION_UNKNOWN)
    FireEventIfStarted();
}

blink::IPPermissionStatus FilteringNetworkManager::GetIPPermissionStatus()
    const {
  if (enumeration_permission() == ENUMERATION_ALLOWED) {
    return media_permission_ ? blink::PERMISSION_GRANTED_WITH_CHECKING
                             : blink::PERMISSION_GRANTED_WITHOUT_CHECKING;
  }

  if (!pending_permission_checks_ &&
      enumeration_permission() == ENUMERATION_BLOCKED) {
    return blink::PERMISSION_DENIED;
  }

  return blink::PERMISSION_UNKNOWN;
}

void FilteringNetworkManager::FireEventIfStarted() {
  if (!start_count_)
    return;

  // Post a task to avoid reentrancy.
  //
  // TODO(crbug.com/787254): Use Frame-based TaskRunner here.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      WTF::BindOnce(&FilteringNetworkManager::SendNetworksChangedSignal,
                    GetWeakPtr()));

  sent_first_update_ = true;
}

void FilteringNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace blink
