// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/p2p/filtering_network_manager.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/media_permission.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/p2p/empty_network_manager.h"
#include "third_party/webrtc/rtc_base/ip_address.h"

using NetworkList = rtc::NetworkManager::NetworkList;
using ::testing::SizeIs;

namespace {

enum EventType {
  MIC_DENIED,      // Receive mic permission denied.
  MIC_GRANTED,     // Receive mic permission granted.
  CAMERA_DENIED,   // Receive camera permission denied.
  CAMERA_GRANTED,  // Receive camera permission granted.
  START_UPDATING,  // Client calls StartUpdating() on FilteringNetworkManager.
  STOP_UPDATING,   // Client calls StopUpdating() on FilteringNetworkManager.
  MOCK_NETWORKS_CHANGED_WITH_NEW_NETWORK,   // MockNetworkManager has signaled
                                            // networks changed event and the
                                            // underlying network is replaced by
                                            // a new one.
  MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK,  // MockNetworkManager has signaled
                                            // networks changed event but the
                                            // underlying network stays the
                                            // same.
};

enum ResultType {
  NO_SIGNAL,                   // Do not expect SignalNetworksChanged fired.
  SIGNAL_ENUMERATION_BLOCKED,  // Expect SignalNetworksChanged and
                               // ENUMERATION_BLOCKED.
  SIGNAL_ENUMERATION_ALLOWED,  // Expect SignalNetworksChanged and
                               // ENUMERATION_ALLOWED.
};

struct TestEntry {
  EventType event;
  ResultType expected_result;
};

class EmptyMdnsResponder : public webrtc::MdnsResponderInterface {
 public:
  void CreateNameForAddress(const rtc::IPAddress& addr,
                            NameCreatedCallback callback) override {
    NOTREACHED();
  }
  void RemoveNameForAddress(const rtc::IPAddress& addr,
                            NameRemovedCallback callback) override {
    NOTREACHED();
  }
};

class MockNetworkManager : public rtc::NetworkManagerBase {
 public:
  MockNetworkManager() : mdns_responder_(new EmptyMdnsResponder()) {}
  // Mimic the current behavior that once the first signal is sent, any future
  // StartUpdating() will trigger another one.
  void StartUpdating() override {
    if (sent_first_update_)
      SignalNetworksChanged();
  }
  void StopUpdating() override {}
  void GetNetworks(NetworkList* networks) const override {
    networks->push_back(network_.get());
  }

  void SendNetworksChanged() {
    sent_first_update_ = true;
    SignalNetworksChanged();
  }

  webrtc::MdnsResponderInterface* GetMdnsResponder() const override {
    return mdns_responder_.get();
  }

  void CopyAndSetNetwork(const rtc::Network& network) {
    network_ = std::make_unique<rtc::Network>(network);
    network_->AddIP(network_->GetBestIP());
  }

 private:
  bool sent_first_update_ = false;
  std::unique_ptr<rtc::Network> network_;
  std::unique_ptr<EmptyMdnsResponder> mdns_responder_;
};

class MockMediaPermission : public media::MediaPermission {
 public:
  MockMediaPermission() {}
  ~MockMediaPermission() override {}

  void RequestPermission(Type type,
                         PermissionStatusCB permission_status_cb) override {
    NOTIMPLEMENTED();
  }

  void HasPermission(Type type,
                     PermissionStatusCB permission_status_cb) override {
    if (type == MediaPermission::AUDIO_CAPTURE) {
      DCHECK(mic_callback_.is_null());
      mic_callback_ = std::move(permission_status_cb);
    } else {
      DCHECK(type == MediaPermission::VIDEO_CAPTURE);
      DCHECK(camera_callback_.is_null());
      camera_callback_ = std::move(permission_status_cb);
    }
  }

  bool IsEncryptedMediaEnabled() override { return true; }

  void SetMicPermission(bool granted) {
    if (!mic_callback_)
      return;

    std::move(mic_callback_).Run(granted);
  }

  void SetCameraPermission(bool granted) {
    if (!camera_callback_)
      return;

    std::move(camera_callback_).Run(granted);
  }

 private:
  PermissionStatusCB mic_callback_;
  PermissionStatusCB camera_callback_;
};

}  // namespace

namespace blink {

class FilteringNetworkManagerTest : public testing::Test,
                                    public sigslot::has_slots<> {
 public:
  FilteringNetworkManagerTest()
      : media_permission_(new MockMediaPermission()),
        task_runner_(new base::TestSimpleTaskRunner()),
        task_runner_handle_(task_runner_) {
    networks_.emplace_back("test_eth0", "Test Network Adapter 1",
                           rtc::IPAddress(0x12345600U), 24,
                           rtc::ADAPTER_TYPE_ETHERNET),
        networks_.back().AddIP(rtc::IPAddress(0x12345678));
    networks_.emplace_back("test_eth1", "Test Network Adapter 2",
                           rtc::IPAddress(0x87654300U), 24,
                           rtc::ADAPTER_TYPE_ETHERNET),
        networks_.back().AddIP(rtc::IPAddress(0x87654321));
  }

  void SetupNetworkManager(bool multiple_routes_requested) {
    base_network_manager_ = std::make_unique<MockNetworkManager>();
    SetNewNetworkForBaseNetworkManager();
    if (multiple_routes_requested) {
      network_manager_ = std::make_unique<FilteringNetworkManager>(
          base_network_manager_.get(), GURL(), media_permission_.get(),
          allow_mdns_obfuscation_);
      network_manager_->Initialize();
    } else {
      network_manager_ = std::make_unique<blink::EmptyNetworkManager>(
          base_network_manager_.get());
    }
    network_manager_->SignalNetworksChanged.connect(
        this, &FilteringNetworkManagerTest::OnNetworksChanged);
  }

  void RunTests(TestEntry* tests, size_t size) {
    for (size_t i = 0; i < size; ++i) {
      EXPECT_EQ(tests[i].expected_result, ProcessEvent(tests[i].event))
          << " in step: " << i;
    }
  }

  void SetNewNetworkForBaseNetworkManager() {
    base_network_manager_->CopyAndSetNetwork(networks_[next_new_network_id_]);
    next_new_network_id_ = (next_new_network_id_ + 1) % networks_.size();
  }

  ResultType ProcessEvent(EventType event) {
    clear_callback_called();
    switch (event) {
      case MIC_DENIED:
      case MIC_GRANTED:
        media_permission_->SetMicPermission(event == MIC_GRANTED);
        break;
      case CAMERA_DENIED:
      case CAMERA_GRANTED:
        media_permission_->SetCameraPermission(event == CAMERA_GRANTED);
        break;
      case START_UPDATING:
        network_manager_->StartUpdating();
        break;
      case STOP_UPDATING:
        network_manager_->StopUpdating();
        break;
      case MOCK_NETWORKS_CHANGED_WITH_NEW_NETWORK:
        SetNewNetworkForBaseNetworkManager();
        base_network_manager_->SendNetworksChanged();
        break;
      case MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK:
        base_network_manager_->SendNetworksChanged();
        break;
    }

    task_runner_->RunUntilIdle();

    if (!callback_called_)
      return NO_SIGNAL;

    if (network_manager_->enumeration_permission() ==
        rtc::NetworkManager::ENUMERATION_BLOCKED) {
      EXPECT_EQ(0u, GetP2PNetworkList().size());
      return SIGNAL_ENUMERATION_BLOCKED;
    }
    EXPECT_EQ(1u, GetP2PNetworkList().size());
    return SIGNAL_ENUMERATION_ALLOWED;
  }

 protected:
  const NetworkList& GetP2PNetworkList() {
    network_list_.clear();
    network_manager_->GetNetworks(&network_list_);
    return network_list_;
  }

  void OnNetworksChanged() { callback_called_ = true; }
  void clear_callback_called() { callback_called_ = false; }
  void set_allow_mdns_obfuscation(bool val) { allow_mdns_obfuscation_ = val; }

  bool callback_called_ = false;
  std::unique_ptr<rtc::NetworkManager> network_manager_;
  std::unique_ptr<MockNetworkManager> base_network_manager_;

  std::unique_ptr<MockMediaPermission> media_permission_;
  bool allow_mdns_obfuscation_ = true;

  std::vector<rtc::Network> networks_;
  int next_new_network_id_ = 0;

  NetworkList network_list_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

// Test that when multiple routes is not requested, SignalNetworksChanged is
// fired right after the StartUpdating().
TEST_F(FilteringNetworkManagerTest, MultipleRoutesNotRequested) {
  SetupNetworkManager(false);
  TestEntry tests[] = {
      // Underneath network manager signals, no callback as StartUpdating() is
      // not called.
      {MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK, NO_SIGNAL},
      // StartUpdating() is called, should receive callback as the multiple
      // routes is not requested.
      {START_UPDATING, SIGNAL_ENUMERATION_BLOCKED},
      // Further network signal should trigger callback, since the
      // EmptyNetworkManager always forwards the signal from the base network
      // manager if there is any outstanding StartUpdate();
      {MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK, SIGNAL_ENUMERATION_BLOCKED},
      // StartUpdating() always triggers callback after we have sent the first
      // network update.
      {START_UPDATING, SIGNAL_ENUMERATION_BLOCKED},
      {STOP_UPDATING, NO_SIGNAL},
      {STOP_UPDATING, NO_SIGNAL},
      // No outstanding StartUpdating(), no more signal.
      {MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK, NO_SIGNAL},
  };

  RunTests(tests, base::size(tests));
}

// Test that multiple routes request is blocked and signaled right after
// StartUpdating() since mic/camera permissions are denied.
TEST_F(FilteringNetworkManagerTest, BlockMultipleRoutesByStartUpdating) {
  SetupNetworkManager(true);

  TestEntry tests[] = {
      {MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK, NO_SIGNAL},
      // Both mic and camera are denied.
      {MIC_DENIED, NO_SIGNAL},
      {CAMERA_DENIED, NO_SIGNAL},
      // Once StartUpdating() is called, signal network changed event with
      // ENUMERATION_BLOCKED.
      {START_UPDATING, SIGNAL_ENUMERATION_BLOCKED},
      // Further network signal should not trigger callback, since the set of
      // networks does not change after merging.
      {MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK, NO_SIGNAL},
      // Signal when observing a change after merging while there is any
      // outstanding StartUpdate();
      {MOCK_NETWORKS_CHANGED_WITH_NEW_NETWORK, SIGNAL_ENUMERATION_BLOCKED},
      {START_UPDATING, SIGNAL_ENUMERATION_BLOCKED},
      {STOP_UPDATING, NO_SIGNAL},
      {STOP_UPDATING, NO_SIGNAL},
  };

  RunTests(tests, base::size(tests));
}

// Test that multiple routes request is blocked and signaled right after
// last pending permission check is denied since StartUpdating() has been called
// previously.
TEST_F(FilteringNetworkManagerTest, BlockMultipleRoutesByPermissionsDenied) {
  SetupNetworkManager(true);

  TestEntry tests[] = {
      // StartUpdating() should not fire the event before we send the first
      // update.
      {START_UPDATING, NO_SIGNAL},
      {MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK, NO_SIGNAL},
      {MIC_DENIED, NO_SIGNAL},
      // The last permission check being denied should immediately trigger the
      // networks changed signal, since we already have an updated network list.
      {CAMERA_DENIED, SIGNAL_ENUMERATION_BLOCKED},
      {START_UPDATING, SIGNAL_ENUMERATION_BLOCKED},
      {STOP_UPDATING, NO_SIGNAL},
      {STOP_UPDATING, NO_SIGNAL},
      // No outstanding StartUpdating(), no more signal.
      {MOCK_NETWORKS_CHANGED_WITH_NEW_NETWORK, NO_SIGNAL},
  };

  RunTests(tests, base::size(tests));
}

// Test that after permissions have been denied, a network change signal from
// the internal NetworkManager is still needed before signaling a network
// change outwards. This is because even if network enumeration is blocked,
// we still want to give time to obtain the default IP addresses.
TEST_F(FilteringNetworkManagerTest, BlockMultipleRoutesByNetworksChanged) {
  SetupNetworkManager(true);

  TestEntry tests[] = {
      {START_UPDATING, NO_SIGNAL},
      {MIC_DENIED, NO_SIGNAL},
      {CAMERA_DENIED, NO_SIGNAL},
      {MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK, SIGNAL_ENUMERATION_BLOCKED},
      {START_UPDATING, SIGNAL_ENUMERATION_BLOCKED},
      {STOP_UPDATING, NO_SIGNAL},
      {STOP_UPDATING, NO_SIGNAL},
  };

  RunTests(tests, base::size(tests));
}

// Test that multiple routes request is granted and signaled right after
// a pending permission check is granted since StartUpdating() has been called
// previously.
TEST_F(FilteringNetworkManagerTest, AllowMultipleRoutesByPermissionsGranted) {
  SetupNetworkManager(true);

  TestEntry tests[] = {
      {START_UPDATING, NO_SIGNAL},
      {MIC_DENIED, NO_SIGNAL},
      {MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK, NO_SIGNAL},
      // Once one media type is granted, signal networks changed with
      // ENUMERATION_ALLOWED.
      {CAMERA_GRANTED, SIGNAL_ENUMERATION_ALLOWED},
      {MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK, NO_SIGNAL},
      {START_UPDATING, SIGNAL_ENUMERATION_ALLOWED},
      {STOP_UPDATING, NO_SIGNAL},
      // If there is any outstanding StartUpdating(), new event from underneath
      // network manger should trigger SignalNetworksChanged.
      {MOCK_NETWORKS_CHANGED_WITH_NEW_NETWORK, SIGNAL_ENUMERATION_ALLOWED},
      {STOP_UPDATING, NO_SIGNAL},
      // No outstanding StartUpdating(), no more signal.
      {MOCK_NETWORKS_CHANGED_WITH_NEW_NETWORK, NO_SIGNAL},
  };

  RunTests(tests, base::size(tests));
}

// Test that multiple routes request is granted and signaled right after
// StartUpdating() since there is at least one media permission granted.
TEST_F(FilteringNetworkManagerTest, AllowMultipleRoutesByStartUpdating) {
  SetupNetworkManager(true);

  TestEntry tests[] = {
      {MIC_DENIED, NO_SIGNAL},
      {MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK, NO_SIGNAL},
      {CAMERA_GRANTED, NO_SIGNAL},
      // StartUpdating() should signal the event with the status of permissions
      // granted.
      {START_UPDATING, SIGNAL_ENUMERATION_ALLOWED},
      {MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK, NO_SIGNAL},
      {START_UPDATING, SIGNAL_ENUMERATION_ALLOWED},
      {STOP_UPDATING, NO_SIGNAL},
      // Signal when observing a change after merging while there is any
      // outstanding StartUpdate();
      {MOCK_NETWORKS_CHANGED_WITH_NEW_NETWORK, SIGNAL_ENUMERATION_ALLOWED},
      {STOP_UPDATING, NO_SIGNAL},
      // No outstanding StartUpdating(), no more signal.
      {MOCK_NETWORKS_CHANGED_WITH_NEW_NETWORK, NO_SIGNAL},
  };

  RunTests(tests, base::size(tests));
}

// Test that multiple routes request is granted and signaled right after
// underneath NetworkManager's SignalNetworksChanged() as at least one
// permission is granted and StartUpdating() has been called.
TEST_F(FilteringNetworkManagerTest, AllowMultipleRoutesByNetworksChanged) {
  SetupNetworkManager(true);

  TestEntry tests[] = {
      {START_UPDATING, NO_SIGNAL},
      {CAMERA_GRANTED, NO_SIGNAL},
      // Underneath network manager's signal networks changed should trigger
      // SignalNetworksChanged with ENUMERATION_ALLOWED.
      {MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK, SIGNAL_ENUMERATION_ALLOWED},
      {MIC_DENIED, NO_SIGNAL},
      {MOCK_NETWORKS_CHANGED_WITH_NEW_NETWORK, SIGNAL_ENUMERATION_ALLOWED},
      {START_UPDATING, SIGNAL_ENUMERATION_ALLOWED},
      {STOP_UPDATING, NO_SIGNAL},
      {MOCK_NETWORKS_CHANGED_WITH_NEW_NETWORK, SIGNAL_ENUMERATION_ALLOWED},
      {STOP_UPDATING, NO_SIGNAL},
      {MOCK_NETWORKS_CHANGED_WITH_NEW_NETWORK, NO_SIGNAL},
  };

  RunTests(tests, base::size(tests));
}

// Test that the networks provided by the GetNetworks() and
// GetAnyAddressNetworks() are not associated with an mDNS responder if the
// enumeration permission is granted, even if the mDNS obfuscation of local IPs
// is allowed (which is by default).
TEST_F(FilteringNetworkManagerTest, NullMdnsResponderAfterPermissionGranted) {
  SetupNetworkManager(true);

  TestEntry setup_steps[] = {
      {MOCK_NETWORKS_CHANGED_WITH_SAME_NETWORK, NO_SIGNAL},
      // Both mic and camera are granted.
      {MIC_GRANTED, NO_SIGNAL},
      {CAMERA_GRANTED, NO_SIGNAL},
      // Once StartUpdating() is called, signal network changed event with
      // ENUMERATION_ALLOWED.
      {START_UPDATING, SIGNAL_ENUMERATION_ALLOWED},
  };
  RunTests(setup_steps, base::size(setup_steps));

  NetworkList networks;
  network_manager_->GetNetworks(&networks);
  EXPECT_THAT(networks, SizeIs(1u));
  for (const rtc::Network* network : networks) {
    EXPECT_EQ(nullptr, network->GetMdnsResponder());
  }

  networks.clear();
  network_manager_->GetAnyAddressNetworks(&networks);
  EXPECT_THAT(networks, SizeIs(2u));
  for (const rtc::Network* network : networks) {
    EXPECT_EQ(nullptr, network->GetMdnsResponder());
  }
}

// Test the networks on the default routes given by GetAnyAddressNetworks() are
// associated with an mDNS responder if the enumeration is blocked and the mDNS
// obfuscation of local IPs is allowed (which is by default).
TEST_F(FilteringNetworkManagerTest,
       ProvideMdnsResponderForDefaultRouteAfterPermissionDenied) {
  SetupNetworkManager(true);
  // By default, the enumeration is blocked if we provide |media_permission_|;
  EXPECT_EQ(rtc::NetworkManager::ENUMERATION_BLOCKED,
            network_manager_->enumeration_permission());

  NetworkList networks;
  network_manager_->GetNetworks(&networks);
  EXPECT_TRUE(networks.empty());

  network_manager_->GetAnyAddressNetworks(&networks);
  EXPECT_THAT(networks, SizeIs(2u));
  EXPECT_NE(nullptr, network_manager_->GetMdnsResponder());
  for (const rtc::Network* network : networks) {
    EXPECT_EQ(network_manager_->GetMdnsResponder(),
              network->GetMdnsResponder());
  }
}

// This is a similar test to the previous one but tests that the networks
// provided by the GetNetworks() and GetAnyAddressNetworks() are not associated
// with an mDNS responder if the mDNS obfuscation of local IPs is not allowed.
TEST_F(FilteringNetworkManagerTest,
       NullMdnsResponderWhenMdnsObfuscationDisallowedAfterPermissionDenied) {
  set_allow_mdns_obfuscation(false);
  SetupNetworkManager(true);
  // By default, the enumeration is blocked if we provide |media_permission_|;
  EXPECT_EQ(rtc::NetworkManager::ENUMERATION_BLOCKED,
            network_manager_->enumeration_permission());

  NetworkList networks;
  network_manager_->GetNetworks(&networks);
  EXPECT_TRUE(networks.empty());

  network_manager_->GetAnyAddressNetworks(&networks);
  EXPECT_THAT(networks, SizeIs(2u));
  for (const rtc::Network* network : networks) {
    EXPECT_EQ(nullptr, network->GetMdnsResponder());
  }
}

}  // namespace blink
