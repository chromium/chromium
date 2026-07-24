// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_advertising_event.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class BluetoothTestHelper {
 public:
  static BluetoothDevice* GetOrCreateBluetoothDevice(
      Bluetooth* bluetooth,
      ScriptState* script_state,
      const mojom::blink::WebBluetoothDevicePtr& device_ptr,
      ExecutionContext* context) {
    return bluetooth->GetOrCreateBluetoothDevice(script_state, device_ptr,
                                                 context);
  }

  static size_t CacheSize(Bluetooth* bluetooth) {
    return bluetooth->device_caches_.size();
  }

  static size_t DefaultCacheSize(Bluetooth* bluetooth) {
    return bluetooth->device_instance_map_.size();
  }

  static void InsertReceiverWorld(Bluetooth* bluetooth,
                                  mojo::ReceiverId id,
                                  DOMWrapperWorld* world) {
    bluetooth->client_receiver_world_map_.insert(id, world);
  }

  static const DOMWrapperWorld* GetWorldForReceiver(Bluetooth* bluetooth,
                                                    mojo::ReceiverId id) {
    auto it = bluetooth->client_receiver_world_map_.find(id);
    if (it == bluetooth->client_receiver_world_map_.end()) {
      return nullptr;
    }
    return it->value.Get();
  }

  static void SetFakeCurrentReceiver(Bluetooth* bluetooth,
                                     std::optional<mojo::ReceiverId> id) {
    bluetooth->fake_current_receiver_for_testing_ = id;
  }

  static size_t ReceiverMapSize(Bluetooth* bluetooth) {
    return bluetooth->client_receiver_world_map_.size();
  }
};

TEST(BluetoothTest, WorldIsolatedCacheEnabled) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Enable feature
  ScopedWebBluetoothWorldIsolatedCacheForTest feature_helper(true);

  Navigator* navigator = scope.GetFrame().DomWindow()->navigator();
  Bluetooth* bluetooth = Bluetooth::bluetooth(*navigator);
  ASSERT_TRUE(bluetooth);

  // Create main world script state
  ScriptState* main_script_state = scope.GetScriptState();
  DOMWrapperWorld& main_world = main_script_state->World();
  EXPECT_TRUE(main_world.IsMainWorld());

  // Create isolated world
  DOMWrapperWorld* isolated_world =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, 1);
  EXPECT_TRUE(isolated_world->IsIsolatedWorld());

  // We need a ScriptState for the isolated world.
  scope.GetFrame().GetWindowProxy(*isolated_world);  // Force initialization
  ScriptState* isolated_script_state =
      ToScriptState(&scope.GetFrame(), *isolated_world);
  ASSERT_TRUE(isolated_script_state);

  auto device_id = WebBluetoothDeviceId::Create();

  auto device_ptr1 = mojom::blink::WebBluetoothDevice::New();
  device_ptr1->id = device_id;
  device_ptr1->name = "Test Device";

  auto device_ptr2 = mojom::blink::WebBluetoothDevice::New();
  device_ptr2->id = device_id;
  device_ptr2->name = "Test Device";

  // GetOrCreateBluetoothDevice in main world
  BluetoothDevice* device_main =
      BluetoothTestHelper::GetOrCreateBluetoothDevice(
          bluetooth, main_script_state, device_ptr1,
          scope.GetExecutionContext());
  ASSERT_TRUE(device_main);

  // GetOrCreateBluetoothDevice in isolated world
  BluetoothDevice* device_isolated =
      BluetoothTestHelper::GetOrCreateBluetoothDevice(
          bluetooth, isolated_script_state, device_ptr2,
          scope.GetExecutionContext());
  ASSERT_TRUE(device_isolated);

  // They should be different C++ objects
  EXPECT_NE(device_main, device_isolated);

  // Cache sizes should be updated
  EXPECT_EQ(BluetoothTestHelper::CacheSize(bluetooth), 2U);
  EXPECT_EQ(BluetoothTestHelper::DefaultCacheSize(bluetooth), 0U);
}

TEST(BluetoothTest, WorldIsolatedCacheDisabled) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Disable feature
  ScopedWebBluetoothWorldIsolatedCacheForTest feature_helper(false);

  Navigator* navigator = scope.GetFrame().DomWindow()->navigator();
  Bluetooth* bluetooth = Bluetooth::bluetooth(*navigator);
  ASSERT_TRUE(bluetooth);

  // Create main world script state
  ScriptState* main_script_state = scope.GetScriptState();
  DOMWrapperWorld& main_world = main_script_state->World();
  EXPECT_TRUE(main_world.IsMainWorld());

  // Create isolated world
  DOMWrapperWorld* isolated_world =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, 1);
  EXPECT_TRUE(isolated_world->IsIsolatedWorld());

  // We need a ScriptState for the isolated world.
  scope.GetFrame().GetWindowProxy(*isolated_world);  // Force initialization
  ScriptState* isolated_script_state =
      ToScriptState(&scope.GetFrame(), *isolated_world);
  ASSERT_TRUE(isolated_script_state);

  auto device_id = WebBluetoothDeviceId::Create();

  auto device_ptr1 = mojom::blink::WebBluetoothDevice::New();
  device_ptr1->id = device_id;
  device_ptr1->name = "Test Device";

  auto device_ptr2 = mojom::blink::WebBluetoothDevice::New();
  device_ptr2->id = device_id;
  device_ptr2->name = "Test Device";

  // GetOrCreateBluetoothDevice in main world
  BluetoothDevice* device_main =
      BluetoothTestHelper::GetOrCreateBluetoothDevice(
          bluetooth, main_script_state, device_ptr1,
          scope.GetExecutionContext());
  ASSERT_TRUE(device_main);

  // GetOrCreateBluetoothDevice in isolated world
  BluetoothDevice* device_isolated =
      BluetoothTestHelper::GetOrCreateBluetoothDevice(
          bluetooth, isolated_script_state, device_ptr2,
          scope.GetExecutionContext());
  ASSERT_TRUE(device_isolated);

  // They should be the same C++ object
  EXPECT_EQ(device_main, device_isolated);

  // Cache sizes should be updated
  EXPECT_EQ(BluetoothTestHelper::CacheSize(bluetooth), 0U);
  EXPECT_EQ(BluetoothTestHelper::DefaultCacheSize(bluetooth), 1U);
}

class TestAdvertisingEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override {
    if (event->type() == event_type_names::kAdvertisementreceived) {
      count_++;
      last_event_ = static_cast<BluetoothAdvertisingEvent*>(event);
    }
  }

  int Count() const { return count_; }
  const BluetoothAdvertisingEvent* LastEvent() const {
    return last_event_.Get();
  }

  void Reset() {
    count_ = 0;
    last_event_ = nullptr;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(last_event_);
    NativeEventListener::Trace(visitor);
  }

 private:
  int count_ = 0;
  Member<BluetoothAdvertisingEvent> last_event_;
};

TEST(BluetoothTest, AdvertisingEventTargetedRouting) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  ScopedWebBluetoothWorldIsolatedCacheForTest feature_helper(true);

  Navigator* navigator = scope.GetFrame().DomWindow()->navigator();
  Bluetooth* bluetooth = Bluetooth::bluetooth(*navigator);
  ASSERT_TRUE(bluetooth);

  // Setup worlds
  ScriptState* main_script_state = scope.GetScriptState();
  DOMWrapperWorld& main_world = main_script_state->World();
  DOMWrapperWorld* isolated_world =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, 1);
  scope.GetFrame().GetWindowProxy(*isolated_world);

  // Add event listener
  auto* listener = MakeGarbageCollected<TestAdvertisingEventListener>();
  bluetooth->addEventListener(event_type_names::kAdvertisementreceived,
                              listener, false);

  // 1. Setup fake IDs and map for Main World and Isolated World
  mojo::ReceiverId id_main = 1234;
  BluetoothTestHelper::InsertReceiverWorld(bluetooth, id_main, &main_world);
  EXPECT_EQ(BluetoothTestHelper::ReceiverMapSize(bluetooth), 1U);
  EXPECT_EQ(BluetoothTestHelper::GetWorldForReceiver(bluetooth, id_main),
            &main_world);

  mojo::ReceiverId id_isolated = 5678;
  BluetoothTestHelper::InsertReceiverWorld(bluetooth, id_isolated,
                                           isolated_world);
  EXPECT_EQ(BluetoothTestHelper::ReceiverMapSize(bluetooth), 2U);
  EXPECT_EQ(BluetoothTestHelper::GetWorldForReceiver(bluetooth, id_isolated),
            isolated_world);

  // Prepare test event
  auto device_id = WebBluetoothDeviceId::Create();
  auto device_ptr = mojom::blink::WebBluetoothDevice::New();
  device_ptr->id = device_id;
  device_ptr->name = "Test Device";
  auto advertising_event_main =
      mojom::blink::WebBluetoothAdvertisingEvent::New();
  advertising_event_main->device = device_ptr.Clone();
  advertising_event_main->name = "Event 1";
  auto advertising_event_isolated =
      mojom::blink::WebBluetoothAdvertisingEvent::New();
  advertising_event_isolated->device = device_ptr.Clone();
  advertising_event_isolated->name = "Event 2";

  // Fire targeted to Main World
  BluetoothTestHelper::SetFakeCurrentReceiver(bluetooth, id_main);
  bluetooth->AdvertisingEvent(std::move(advertising_event_main));

  // Listener should receive exactly ONE event, and it should be tagged for Main
  // World
  EXPECT_EQ(listener->Count(), 1);
  ASSERT_TRUE(listener->LastEvent());
  EXPECT_TRUE(listener->LastEvent()->CanBeDispatchedInWorld(main_world));
  EXPECT_FALSE(listener->LastEvent()->CanBeDispatchedInWorld(*isolated_world));
  listener->Reset();

  // Fire targeted to Isolated World
  BluetoothTestHelper::SetFakeCurrentReceiver(bluetooth, id_isolated);
  bluetooth->AdvertisingEvent(std::move(advertising_event_isolated));

  // Listener should receive exactly ONE event, and it should be tagged for
  // Isolated World
  EXPECT_EQ(listener->Count(), 1);
  ASSERT_TRUE(listener->LastEvent());
  EXPECT_FALSE(listener->LastEvent()->CanBeDispatchedInWorld(main_world));
  EXPECT_TRUE(listener->LastEvent()->CanBeDispatchedInWorld(*isolated_world));
  listener->Reset();

  // 3. Test CancelScan
  bluetooth->CancelScan(id_main);
  EXPECT_EQ(BluetoothTestHelper::ReceiverMapSize(bluetooth), 1U);
  EXPECT_FALSE(BluetoothTestHelper::GetWorldForReceiver(bluetooth, id_main));
  EXPECT_TRUE(BluetoothTestHelper::GetWorldForReceiver(bluetooth, id_isolated));

  // 4. Test PageVisibilityChanged
  bluetooth->PageVisibilityChanged();
  EXPECT_EQ(BluetoothTestHelper::ReceiverMapSize(bluetooth), 0U);

  // Reset fake receiver
  BluetoothTestHelper::SetFakeCurrentReceiver(bluetooth, std::nullopt);
}

}  // namespace blink
