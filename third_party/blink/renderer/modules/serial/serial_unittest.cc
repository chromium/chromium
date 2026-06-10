// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial.h"

#include "third_party/blink/public/mojom/serial/serial.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_unsignedlong.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_serial_port_filter.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/serial/serial_port.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

constexpr uint16_t kTestVendorId = 0x0001;
constexpr uint16_t kTestProductId = 0x0002;
constexpr char kTestServiceClassId[] = "05079c61-147f-473d-8127-fab1bbad7e1a";

}  // namespace

TEST(SerialTest, CreateMojoFilter_EmptyFilter) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  SerialPortFilter* js_filter = SerialPortFilter::Create(scope.GetIsolate());

  mojom::blink::SerialPortFilterPtr mojo_filter =
      Serial::CreateMojoFilter(js_filter, scope.GetExceptionState());
  EXPECT_FALSE(mojo_filter);
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ToExceptionCode(ESErrorType::kTypeError),
            scope.GetExceptionState().Code());
}

TEST(SerialTest, CreateMojoFilter_VendorId) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  SerialPortFilter* js_filter = SerialPortFilter::Create(scope.GetIsolate());
  js_filter->setUsbVendorId(kTestVendorId);

  mojom::blink::SerialPortFilterPtr mojo_filter =
      Serial::CreateMojoFilter(js_filter, scope.GetExceptionState());
  ASSERT_TRUE(mojo_filter);
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(mojo_filter->has_vendor_id);
  EXPECT_EQ(kTestVendorId, mojo_filter->vendor_id);
  EXPECT_FALSE(mojo_filter->has_product_id);
  EXPECT_FALSE(mojo_filter->bluetooth_service_class_id);
}

TEST(SerialTest, CreateMojoFilter_ProductNoVendorId) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  SerialPortFilter* js_filter = SerialPortFilter::Create(scope.GetIsolate());
  // If the filter has a product ID then it must also have a vendor ID.
  js_filter->setUsbProductId(kTestProductId);

  mojom::blink::SerialPortFilterPtr mojo_filter =
      Serial::CreateMojoFilter(js_filter, scope.GetExceptionState());
  EXPECT_FALSE(mojo_filter);
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ToExceptionCode(ESErrorType::kTypeError),
            scope.GetExceptionState().Code());
}

TEST(SerialTest, CreateMojoFilter_BluetoothServiceClassAndVendorId) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  SerialPortFilter* js_filter = SerialPortFilter::Create(scope.GetIsolate());
  // Can't have both Bluetooth and USB filter parameters.
  V8UnionStringOrUnsignedLong* uuid =
      MakeGarbageCollected<V8UnionStringOrUnsignedLong>(kTestServiceClassId);
  js_filter->setUsbVendorId(kTestVendorId);
  js_filter->setBluetoothServiceClassId(uuid);

  mojom::blink::SerialPortFilterPtr mojo_filter =
      Serial::CreateMojoFilter(js_filter, scope.GetExceptionState());
  EXPECT_FALSE(mojo_filter);
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ToExceptionCode(ESErrorType::kTypeError),
            scope.GetExceptionState().Code());
}

TEST(SerialTest, CreateMojoFilter_BluetoothServiceClassAndProductId) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  SerialPortFilter* js_filter = SerialPortFilter::Create(scope.GetIsolate());
  // Can't have both Bluetooth and USB filter parameters.
  js_filter->setUsbProductId(kTestProductId);
  js_filter->setBluetoothServiceClassId(
      MakeGarbageCollected<V8UnionStringOrUnsignedLong>(kTestServiceClassId));

  mojom::blink::SerialPortFilterPtr mojo_filter =
      Serial::CreateMojoFilter(js_filter, scope.GetExceptionState());
  EXPECT_FALSE(mojo_filter);
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ToExceptionCode(ESErrorType::kTypeError),
            scope.GetExceptionState().Code());
}

TEST(SerialTest, CreateMojoFilter_BluetoothServiceClass) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  SerialPortFilter* js_filter = SerialPortFilter::Create(scope.GetIsolate());
  js_filter->setBluetoothServiceClassId(
      MakeGarbageCollected<V8UnionStringOrUnsignedLong>(kTestServiceClassId));

  mojom::blink::SerialPortFilterPtr mojo_filter =
      Serial::CreateMojoFilter(js_filter, scope.GetExceptionState());
  ASSERT_TRUE(mojo_filter);
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  EXPECT_FALSE(mojo_filter->has_vendor_id);
  EXPECT_FALSE(mojo_filter->has_product_id);
  ASSERT_TRUE(mojo_filter->bluetooth_service_class_id);
  EXPECT_EQ(kTestServiceClassId, mojo_filter->bluetooth_service_class_id);
}

TEST(SerialTest, CreateMojoFilter_InvalidBluetoothServiceClass) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  SerialPortFilter* js_filter = SerialPortFilter::Create(scope.GetIsolate());
  js_filter->setBluetoothServiceClassId(
      MakeGarbageCollected<V8UnionStringOrUnsignedLong>("invalid-uuid"));

  mojom::blink::SerialPortFilterPtr mojo_filter =
      Serial::CreateMojoFilter(js_filter, scope.GetExceptionState());
  EXPECT_FALSE(mojo_filter);
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ToExceptionCode(ESErrorType::kTypeError),
            scope.GetExceptionState().Code());
}

class SerialTestHelper {
 public:
  static SerialPort* GetOrCreatePort(Serial* serial,
                                     ScriptState* script_state,
                                     mojom::blink::SerialPortInfoPtr info) {
    return serial->GetOrCreatePort(script_state, std::move(info));
  }
  static size_t CacheSize(Serial* serial) {
    return serial->port_caches_.size();
  }
  static size_t CacheSizeForWorld(Serial* serial, DOMWrapperWorld& world) {
    auto it = serial->port_caches_.find(&world);
    if (it != serial->port_caches_.end()) {
      return it->value->port_cache().size();
    }
    return 0;
  }
  static size_t DefaultCacheSize(Serial* serial) {
    return serial->port_cache_.size();
  }
};

// Verifies that when WebSerialWorldIsolatedCache is enabled, SerialPort
// instances are cached per-world. Requesting the same port token in different
// worlds (main vs. isolated) must return different C++ objects.
TEST(SerialTest, WorldIsolatedCache) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Enable feature
  ScopedWebSerialWorldIsolatedCacheForTest feature_helper(true);

  Navigator* navigator = scope.GetFrame().DomWindow()->navigator();
  Serial* serial = Serial::serial(*navigator);
  ASSERT_TRUE(serial);

  // Create main world script state
  ScriptState* main_script_state = scope.GetScriptState();
  DOMWrapperWorld& main_world = main_script_state->World();
  EXPECT_TRUE(main_world.IsMainWorld());

  // Create isolated world
  DOMWrapperWorld* isolated_world =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, 1);
  EXPECT_TRUE(isolated_world->IsIsolatedWorld());

  // We need a ScriptState for the isolated world.
  // To do that, we can get the WindowProxy for the isolated world.
  scope.GetFrame().GetWindowProxy(*isolated_world);  // Force initialization
  ScriptState* isolated_script_state =
      ToScriptState(&scope.GetFrame(), *isolated_world);
  ASSERT_TRUE(isolated_script_state);

  // We use the same token for both port infos to simulate two different worlds
  // requesting access to the exact same physical serial device.
  auto token = base::UnguessableToken::Create();

  // Create port info
  auto info1 = mojom::blink::SerialPortInfo::New();
  info1->token = token;
  info1->connected = true;

  auto info2 = mojom::blink::SerialPortInfo::New();
  info2->token = token;
  info2->connected = true;

  // GetOrCreatePort in main world
  SerialPort* port_main = SerialTestHelper::GetOrCreatePort(
      serial, main_script_state, std::move(info1));
  ASSERT_TRUE(port_main);

  // GetOrCreatePort in isolated world
  SerialPort* port_isolated = SerialTestHelper::GetOrCreatePort(
      serial, isolated_script_state, std::move(info2));
  ASSERT_TRUE(port_isolated);

  // They should be different C++ objects
  EXPECT_NE(port_main, port_isolated);

  // Cache sizes should be updated
  EXPECT_EQ(SerialTestHelper::CacheSize(serial), 2U);
  EXPECT_EQ(SerialTestHelper::DefaultCacheSize(serial), 0U);
}

// Verifies that when WebSerialWorldIsolatedCache is disabled, the cache
// falls back to the legacy behavior where requesting the same port token in
// different worlds returns the exact same C++ object.
TEST(SerialTest, WorldIsolatedCacheDisabled) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Disable feature
  ScopedWebSerialWorldIsolatedCacheForTest feature_helper(false);

  Navigator* navigator = scope.GetFrame().DomWindow()->navigator();
  Serial* serial = Serial::serial(*navigator);
  ASSERT_TRUE(serial);

  ScriptState* main_script_state = scope.GetScriptState();

  DOMWrapperWorld* isolated_world =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, 1);
  scope.GetFrame().GetWindowProxy(*isolated_world);  // Force initialization
  ScriptState* isolated_script_state =
      ToScriptState(&scope.GetFrame(), *isolated_world);
  ASSERT_TRUE(isolated_script_state);

  // We use the same token for both port infos to simulate two different worlds
  // requesting access to the exact same physical serial device.
  auto token = base::UnguessableToken::Create();

  auto info1 = mojom::blink::SerialPortInfo::New();
  info1->token = token;
  info1->connected = true;

  auto info2 = mojom::blink::SerialPortInfo::New();
  info2->token = token;
  info2->connected = true;

  // GetOrCreatePort in main world
  SerialPort* port_main = SerialTestHelper::GetOrCreatePort(
      serial, main_script_state, std::move(info1));
  ASSERT_TRUE(port_main);

  // GetOrCreatePort in isolated world
  SerialPort* port_isolated = SerialTestHelper::GetOrCreatePort(
      serial, isolated_script_state, std::move(info2));
  ASSERT_TRUE(port_isolated);

  // They should be the SAME C++ object
  EXPECT_EQ(port_main, port_isolated);

  // Cache sizes should be updated
  EXPECT_EQ(SerialTestHelper::CacheSize(serial), 0U);
  EXPECT_EQ(SerialTestHelper::DefaultCacheSize(serial), 1U);
}

// Verifies the entire lifecycle and memory safety of the weak-caching
// mechanism. Specifically, it ensures that:
// 1. SerialPort objects can be garbage-collected when no longer in use.
// 2. The cache automatically removes dead entries to prevent memory leaks.
// 3. Requesting a previously GCed port safely triggers new instance creation.
TEST(SerialTest, GCedPortIsRecreated) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  Navigator* navigator = scope.GetFrame().DomWindow()->navigator();
  Serial* serial = Serial::serial(*navigator);
  ASSERT_TRUE(serial);

  ScriptState* script_state = scope.GetScriptState();
  DOMWrapperWorld& main_world = script_state->World();
  auto token = base::UnguessableToken::Create();

  // We store the port in a WeakPersistent, which does not keep the port alive
  // during GC.
  WeakPersistent<SerialPort> weak_port;
  {
    auto info = mojom::blink::SerialPortInfo::New();
    info->token = token;
    info->connected = true;
    // Requesting the port creates it and caches it.
    SerialPort* port = SerialTestHelper::GetOrCreatePort(serial, script_state,
                                                         std::move(info));
    ASSERT_TRUE(port);
    weak_port = port;
  }

  // Exiting this scope destroys the strong C++ pointer 'port'. Now, only
  // weak references (in the cache and 'weak_port') point to the SerialPort.
  EXPECT_EQ(SerialTestHelper::CacheSizeForWorld(serial, main_world), 1U);

  // Force Oilpan garbage collection. Since there are no active strong
  // references (JS or C++) to the SerialPort, it will be collected, and
  // Oilpan's weak processing should automatically remove the dead entry from
  // the cache map.
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);

  // The port should be GCed
  EXPECT_FALSE(weak_port);

  // Cache should be empty now
  EXPECT_EQ(SerialTestHelper::CacheSizeForWorld(serial, main_world), 0U);

  // GetOrCreatePort again
  auto info2 = mojom::blink::SerialPortInfo::New();
  info2->token = token;
  info2->connected = true;
  SerialPort* port2 =
      SerialTestHelper::GetOrCreatePort(serial, script_state, std::move(info2));

  // It should be non-null (a new port)
  ASSERT_TRUE(port2);
  EXPECT_NE(port2, weak_port);

  EXPECT_EQ(SerialTestHelper::CacheSizeForWorld(serial, main_world), 1U);
}

}  // namespace blink
