// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial.h"

#include "third_party/blink/public/mojom/serial/serial.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_unsignedlong.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_serial_port_filter.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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
  EXPECT_EQ(kTestServiceClassId, mojo_filter->bluetooth_service_class_id->uuid);
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

}  // namespace blink
