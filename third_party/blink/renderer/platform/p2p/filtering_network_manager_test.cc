// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/p2p/filtering_network_manager.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "media/base/media_permission.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/p2p/empty_network_manager.h"
#include "third_party/webrtc/rtc_base/ip_address.h"

using ::testing::SizeIs;

namespace {

enum EventType {
  kMicDenied,      // Receive mic permission denied.
  kMicGranted,     // Receive mic permission granted.
  kCameraDenied,   // Receive camera permission denied.
  kCameraGranted,  // Receive camera permission granted.
  kStartUpdating,  // Client calls StartUpdating() on FilteringNetworkManager.
  kStopUpdating,   // Client calls StopUpdating() on FilteringNetworkManager.
  kMockNetworksChangedWithNewNetwork,   // MockNetworkManager has signaled
                                        // networks changed event and the
                                        // underlying network is replaced by
                                        // a new one.
  kMockNetworksChangedWithSameNetwork,  // MockNetworkManager has signaled
                                        // networks changed event but the
                                        // underlying network stays the
                                        // same.
};

enum ResultType {
  kNoSignal,                  // Do not expect SignalNetworksChanged fired.
  kSignalEnumerationBlocked,  // Expect SignalNetworksChanged and
                              // ENUMERATION_BLOCKED.
  kSignalEnumerationAllowed,  // Expect SignalNetworksChanged and
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
    NOTREACHED_IN_MIGRATION();
  }
  void RemoveNameForAddress(const rtc::IPAddress& addr,
                            NameRemovedCallback callback) override {
    NOTREACHED_IN_MIGRATION();
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

  std::vector<const rtc::Network*> GetNetworks() const override {
    return {network_.get()};
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

  base::WeakPtr<MockNetworkManager> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  bool sent_first_update_ = false;
  std::unique_ptr<rtc::Network> network_;
  std::unique_ptr<EmptyMdnsResponder> mdns_responder_;
  base::WeakPtrFactory<MockNetworkManager> weak_factory_{this};
};

class MockMediaPermission : public media::MediaPermission {
 public:
  MockMediaPermission() = default;
  ~MockMediaPermission() override = default;

  void RequestPermission(Type type,
                         PermissionStatusCB permission_status_cb) override {
    NOTIMPLEMENTED();
  }

  void HasPermission(Type type,
                     PermissionStatusCB permission_status_cb) override {
    if (type == MediaPermission::Type::kAudioCapture) {
      DCHECK(mic_callback_.is_null());
      mic_callback_ = std::move(permission_status_cb);
    } else {
      DCHECK(type == MediaPermission::Type::kVideoCapture);
      DCHECK(camera_callback_.is_null());
      camera_callback_ = std::move(permission_status_cb);
    }
  }

  bool IsEncryptedMediaEnabled() override { return true; }

#if BUILDFLAG(IS_WIN)
  void IsHardwareSecureDecryptionAllowed(
      IsHardwareSecureDecryptionAllowedCB cb) override {
    std::move(cb).Run(true);
  }
#endif  // BUILDFLAG(IS_WIN)

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
        task_runner_current_default_handle_(task_runner_) {
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
      network_manager_.reset(new FilteringNetworkManager(
          base_network_manager_->AsWeakPtr(), media_permission_.get(),
          allow_mdns_obfuscation_));
      network_manager_->Initialize();
    } else {
      network_manager_.reset(new EmptyNetworkManager(
          base_network_manager_.get(), base_network_manager_->AsWeakPtr()));
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
      case kMicDenied:
      case kMicGranted:
        media_permission_->SetMicPermission(event == kMicGranted);
        break;
      case kCameraDenied:
      case kCameraGranted:
        media_permission_->SetCameraPermission(event == kCameraGranted);
        break;
      case kStartUpdating:
        network_manager_->StartUpdating();
        break;
      case kStopUpdating:
        network_manager_->StopUpdating();
        break;
      case kMockNetworksChangedWithNewNetwork:
        SetNewNetworkForBaseNetworkManager();
        base_network_manager_->SendNetworksChanged();
        break;
      case kMockNetworksChangedWithSameNetwork:
        base_network_manager_->SendNetworksChanged();
        break;
    }

    task_runner_->RunUntilIdle();

    if (!callback_called_)
      return kNoSignal;

    if (network_manager_->enumeration_permission() ==
        rtc::NetworkManager::ENUMERATION_BLOCKED) {
      EXPECT_EQ(0u, GetP2PNetworkList().size());
      return kSignalEnumerationBlocked;
    }
    EXPECT_EQ(1u, GetP2PNetworkList().size());
    return kSignalEnumerationAllowed;
  }

 protected:
  const std::vector<const rtc::Network*>& GetP2PNetworkList() {
    network_list_ = network_manager_->GetNetworks();
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

  // This field is not vector<raw_ptr<...>> due to interaction with third_party
  // api.
  RAW_PTR_EXCLUSION std::vector<const rtc::Network*> network_list_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
};

// Test that when multiple routes is not requested, SignalNetworksChanged is
// fired right after the StartUpdating().
TEST_F(FilteringNetworkManagerTest, MultipleRoutesNotRequested) {
  SetupNetworkManager(false);
  TestEntry tests[] = {
      // Underneath network manager signals, no callback as StartUpdating() is
      // not called.
      {kMockNetworksChangedWithSameNetwork, kNoSignal},
      // StartUpdating() is called, should receive callback as the multiple
      // routes is not requested.
      {kStartUpdating, kSignalEnumerationBlocked},
      // Further network signal should trigger callback, since the
      // EmptyNetworkManager always forwards the signal from the base network
      // manager if there is any outstanding StartUpdate();
      {kMockNetworksChangedWithSameNetwork, kSignalEnumerationBlocked},
      // StartUpdating() always triggers callback after we have sent the first
      // network update.
      {kStartUpdating, kSignalEnumerationBlocked},
      {kStopUpdating, kNoSignal},
      {kStopUpdating, kNoSignal},
      // No outstanding StartUpdating(), no more signal.
      {kMockNetworksChangedWithSameNetwork, kNoSignal},
  };

  RunTests(tests, std::size(tests));
}

// Test that multiple routes request is blocked and signaled right after
// StartUpdating() since mic/camera permissions are denied.
TEST_F(FilteringNetworkManagerTest, BlockMultipleRoutesByStartUpdating) {
  SetupNetworkManager(true);

  TestEntry tests[] = {
      {kMockNetworksChangedWithSameNetwork, kNoSignal},
      // Both mic and camera are denied.
      {kMicDenied, kNoSignal},
      {kCameraDenied, kNoSignal},
      // Once StartUpdating() is called, signal network changed event with
      // ENUMERATION_BLOCKED.
      {kStartUpdating, kSignalEnumerationBlocked},
      // Further network signal should not trigger callback, since the set of
      // networks does not change after merging.
      {kMockNetworksChangedWithSameNetwork, kNoSignal},
      // Signal when observing a change after merging while there is any
      // outstanding StartUpdate();
      {kMockNetworksChangedWithNewNetwork, kSignalEnumerationBlocked},
      {kStartUpdating, kSignalEnumerationBlocked},
      {kStopUpdating, kNoSignal},
      {kStopUpdating, kNoSignal},
  };

  RunTests(tests, std::size(tests));
}

// Test that multiple routes request is blocked and signaled right after
// last pending permission check is denied since StartUpdating() has been called
// previously.
TEST_F(FilteringNetworkManagerTest, BlockMultipleRoutesByPermissionsDenied) {
  SetupNetworkManager(true);

  TestEntry tests[] = {
      // StartUpdating() should not fire the event before we send the first
      // update.
      {kStartUpdating, kNoSignal},
      {kMockNetworksChangedWithSameNetwork, kNoSignal},
      {kMicDenied, kNoSignal},
      // The last permission check being denied should immediately trigger the
      // networks changed signal, since we already have an updated network list.
      {kCameraDenied, kSignalEnumerationBlocked},
      {kStartUpdating, kSignalEnumerationBlocked},
      {kStopUpdating, kNoSignal},
      {kStopUpdating, kNoSignal},
      // No outstanding StartUpdating(), no more signal.
      {kMockNetworksChangedWithNewNetwork, kNoSignal},
  };

  RunTests(tests, std::size(tests));
}

// Test that after permissions have been denied, a network change signal from
// the internal NetworkManager is still needed before signaling a network
// change outwards. This is because even if network enumeration is blocked,
// we still want to give time to obtain the default IP addresses.
TEST_F(FilteringNetworkManagerTest, BlockMultipleRoutesByNetworksChanged) {
  SetupNetworkManager(true);

  TestEntry tests[] = {
      {kStartUpdating, kNoSignal},
      {kMicDenied, kNoSignal},
      {kCameraDenied, kNoSignal},
      {kMockNetworksChangedWithSameNetwork, kSignalEnumerationBlocked},
      {kStartUpdating, kSignalEnumerationBlocked},
      {kStopUpdating, kNoSignal},
      {kStopUpdating, kNoSignal},
  };

  RunTests(tests, std::size(tests));
}

// Test that multiple routes request is granted and signaled right after
// a pending permission check is granted since StartUpdating() has been called
// previously.
TEST_F(FilteringNetworkManagerTest, AllowMultipleRoutesByPermissionsGranted) {
  SetupNetworkManager(true);

  TestEntry tests[] = {
      {kStartUpdating, kNoSignal},
      {kMicDenied, kNoSignal},
      {kMockNetworksChangedWithSameNetwork, kNoSignal},
      // Once one media type is granted, signal networks changed with
      // ENUMERATION_ALLOWED.
      {kCameraGranted, kSignalEnumerationAllowed},
      {kMockNetworksChangedWithSameNetwork, kNoSignal},
      {kStartUpdating, kSignalEnumerationAllowed},
      {kStopUpdating, kNoSignal},
      // If there is any outstanding StartUpdating(), new event from underneath
      // network manger should trigger SignalNetworksChanged.
      {kMockNetworksChangedWithNewNetwork, kSignalEnumerationAllowed},
      {kStopUpdating, kNoSignal},
      // No outstanding StartUpdating(), no more signal.
      {kMockNetworksChangedWithNewNetwork, kNoSignal},
  };

  RunTests(tests, std::size(tests));
}

// Test that multiple routes request is granted and signaled right after
// StartUpdating() since there is at least one media permission granted.
TEST_F(FilteringNetworkManagerTest, AllowMultipleRoutesByStartUpdating) {
  SetupNetworkManager(true);

  TestEntry tests[] = {
      {kMicDenied, kNoSignal},
      {kMockNetworksChangedWithSameNetwork, kNoSignal},
      {kCameraGranted, kNoSignal},
      // StartUpdating() should signal the event with the status of permissions
      // granted.
      {kStartUpdating, kSignalEnumerationAllowed},
      {kMockNetworksChangedWithSameNetwork, kNoSignal},
      {kStartUpdating, kSignalEnumerationAllowed},
      {kStopUpdating, kNoSignal},
      // Signal when observing a change after merging while there is any
      // outstanding StartUpdate();
      {kMockNetworksChangedWithNewNetwork, kSignalEnumerationAllowed},
      {kStopUpdating, kNoSignal},
      // No outstanding StartUpdating(), no more signal.
      {kMockNetworksChangedWithNewNetwork, kNoSignal},
  };

  RunTests(tests, std::size(tests));
}

// Test that multiple routes request is granted and signaled right after
// underneath NetworkManager's SignalNetworksChanged() as at least one
// permission is granted and StartUpdating() has been called.
TEST_F(FilteringNetworkManagerTest, AllowMultipleRoutesByNetworksChanged) {
  SetupNetworkManager(true);

  TestEntry tests[] = {
      {kStartUpdating, kNoSignal},
      {kCameraGranted, kNoSignal},
      // Underneath network manager's signal networks changed should trigger
      // SignalNetworksChanged with ENUMERATION_ALLOWED.
      {kMockNetworksChangedWithSameNetwork, kSignalEnumerationAllowed},
      {kMicDenied, kNoSignal},
      {kMockNetworksChangedWithNewNetwork, kSignalEnumerationAllowed},
      {kStartUpdating, kSignalEnumerationAllowed},
      {kStopUpdating, kNoSignal},
      {kMockNetworksChangedWithNewNetwork, kSignalEnumerationAllowed},
      {kStopUpdating, kNoSignal},
      {kMockNetworksChangedWithNewNetwork, kNoSignal},
  };

  RunTests(tests, std::size(tests));
}

// Test that the networks provided by the GetNetworks() and
// GetAnyAddressNetworks() are not associated with an mDNS responder if the
// enumeration permission is granted, even if the mDNS obfuscation of local IPs
// is allowed (which is by default).
TEST_F(FilteringNetworkManagerTest, NullMdnsResponderAfterPermissionGranted) {
  SetupNetworkManager(true);

  TestEntry setup_steps[] = {
      {kMockNetworksChangedWithSameNetwork, kNoSignal},
      // Both mic and camera are granted.
      {kMicGranted, kNoSignal},
      {kCameraGranted, kNoSignal},
      // Once StartUpdating() is called, signal network changed event with
      // ENUMERATION_ALLOWED.
      {kStartUpdating, kSignalEnumerationAllowed},
  };
  RunTests(setup_steps, std::size(setup_steps));

  std::vector<const rtc::Network*> networks = network_manager_->GetNetworks();
  EXPECT_THAT(networks, SizeIs(1u));
  for (const rtc::Network* network : networks) {
    EXPECT_EQ(nullptr, network->GetMdnsResponder());
  }

  networks = network_manager_->GetAnyAddressNetworks();
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

  std::vector<const rtc::Network*> networks = network_manager_->GetNetworks();
  EXPECT_TRUE(networks.empty());

  networks = network_manager_->GetAnyAddressNetworks();
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

  std::vector<const rtc::Network*> networks = network_manager_->GetNetworks();
  EXPECT_TRUE(networks.empty());

  networks = network_manager_->GetAnyAddressNetworks();
  EXPECT_THAT(networks, SizeIs(2u));
  for (const rtc::Network* network : networks) {
    EXPECT_EQ(nullptr, network->GetMdnsResponder());
  }
}

}  // namespace blink
