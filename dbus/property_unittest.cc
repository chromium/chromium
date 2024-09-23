// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "dbus/property.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/test_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {

// The property test exerises the asynchronous APIs in PropertySet and
// Property<>.
class PropertyTest : public testing::Test {
 public:
  PropertyTest() = default;

  struct Properties : public PropertySet {
    Property<std::string> name;
    Property<int16_t> version;
    Property<std::vector<std::string>> methods;
    Property<std::vector<ObjectPath>> objects;
    Property<std::vector<uint8_t>> bytes;

    Properties(ObjectProxy* object_proxy,
               PropertyChangedCallback property_changed_callback)
        : PropertySet(object_proxy,
                      "org.chromium.TestInterface",
                      property_changed_callback) {
      RegisterProperty("Name", &name);
      RegisterProperty("Version", &version);
      RegisterProperty("Methods", &methods);
      RegisterProperty("Objects", &objects);
      RegisterProperty("Bytes", &bytes);
    }
  };

  void SetUp() override {
    // Make the main thread not to allow IO.
    disallow_blocking_.emplace();

    // Start the D-Bus thread.
    dbus_thread_ = std::make_unique<base::Thread>("D-Bus Thread");
    base::Thread::Options thread_options;
    thread_options.message_pump_type = base::MessagePumpType::IO;
    ASSERT_TRUE(dbus_thread_->StartWithOptions(std::move(thread_options)));

    // Start the test service, using the D-Bus thread.
    TestService::Options options;
    options.dbus_task_runner = dbus_thread_->task_runner();
    test_service_ = std::make_unique<TestService>(options);
    ASSERT_TRUE(test_service_->StartService());
    test_service_->WaitUntilServiceIsStarted();
    ASSERT_TRUE(test_service_->HasDBusThread());

    // Create the client, using the D-Bus thread.
    Bus::Options bus_options;
    bus_options.bus_type = Bus::SESSION;
    bus_options.connection_type = Bus::PRIVATE;
    bus_options.dbus_task_runner = dbus_thread_->task_runner();
    bus_ = new Bus(bus_options);
    object_proxy_ = bus_->GetObjectProxy(
        test_service_->service_name(),
        ObjectPath("/org/chromium/TestObject"));
    ASSERT_TRUE(bus_->HasDBusThread());

    // Create the properties structure
    properties_ = std::make_unique<Properties>(
        object_proxy_, base::BindRepeating(&PropertyTest::OnPropertyChanged,
                                           base::Unretained(this)));
    properties_->ConnectSignals();
    properties_->GetAll();
  }

  void TearDown() override {
    bus_->ShutdownOnDBusThreadAndBlock();

    // Shut down the service.
    test_service_->ShutdownAndBlock();

    // Stopping a thread is considered an IO operation, so do this after
    // allowing IO.
    disallow_blocking_.reset();
    test_service_->Stop();
  }

  // Generic callback, bind with a string |id| for passing to
  // WaitForCallback() to ensure the callback for the right method is
  // waited for.
  void PropertyCallback(const std::string& id, bool success) {
    last_callback_ = id;
    run_loop_->Quit();
  }

  // Generic method callback, that might be used together with
  // WaitForMethodCallback to test wether method was succesfully called.
  void MethodCallback(Response* response) { run_loop_->Quit(); }

 protected:
  // Called when a property value is updated.
  void OnPropertyChanged(const std::string& name) {
    updated_properties_.push_back(name);
    run_loop_->Quit();
  }

  // Waits for the given number of updates.
  void WaitForUpdates(size_t num_updates) {
    while (updated_properties_.size() < num_updates) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
    for (size_t i = 0; i < num_updates; ++i)
      updated_properties_.erase(updated_properties_.begin());
  }

  // Name, Version, Methods, Objects
  static const int kExpectedSignalUpdates = 5;

  // Waits for initial values to be set.
  void WaitForGetAll() {
    WaitForUpdates(kExpectedSignalUpdates);
  }

  // Waits until MethodCallback is called.
  void WaitForMethodCallback() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // Waits for the callback. |id| is the string bound to the callback when
  // the method call is made that identifies it and distinguishes from any
  // other; you can set this to whatever you wish.
  void WaitForCallback(const std::string& id) {
    while (last_callback_ != id) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::optional<base::ScopedDisallowBlocking> disallow_blocking_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<base::Thread> dbus_thread_;
  scoped_refptr<Bus> bus_;
  raw_ptr<ObjectProxy, AcrossTasksDanglingUntriaged> object_proxy_;
  std::unique_ptr<Properties> properties_;
  std::unique_ptr<TestService> test_service_;
  // Properties updated.
  std::vector<std::string> updated_properties_;
  // Last callback received.
  std::string last_callback_;
};

TEST_F(PropertyTest, InitialValues) {
  EXPECT_FALSE(properties_->name.is_valid());
  EXPECT_FALSE(properties_->version.is_valid());

  WaitForGetAll();

  EXPECT_TRUE(properties_->name.is_valid());
  EXPECT_EQ("TestService", properties_->name.value());
  EXPECT_TRUE(properties_->version.is_valid());
  EXPECT_EQ(10, properties_->version.value());

  std::vector<std::string> methods = properties_->methods.value();
  ASSERT_EQ(4U, methods.size());
  EXPECT_EQ("Echo", methods[0]);
  EXPECT_EQ("SlowEcho", methods[1]);
  EXPECT_EQ("AsyncEcho", methods[2]);
  EXPECT_EQ("BrokenMethod", methods[3]);

  std::vector<ObjectPath> objects = properties_->objects.value();
  ASSERT_EQ(1U, objects.size());
  EXPECT_EQ(ObjectPath("/TestObjectPath"), objects[0]);

  std::vector<uint8_t> bytes = properties_->bytes.value();
  ASSERT_EQ(4U, bytes.size());
  EXPECT_EQ('T', bytes[0]);
  EXPECT_EQ('e', bytes[1]);
  EXPECT_EQ('s', bytes[2]);
  EXPECT_EQ('t', bytes[3]);
}

TEST_F(PropertyTest, UpdatedValues) {
  WaitForGetAll();

  // Update the value of the "Name" property, this value should not change.
  properties_->name.Get(base::BindOnce(&PropertyTest::PropertyCallback,
                                       base::Unretained(this), "Name"));
  WaitForCallback("Name");
  WaitForUpdates(1);

  EXPECT_EQ("TestService", properties_->name.value());

  // Update the value of the "Version" property, this value should be changed.
  properties_->version.Get(base::BindOnce(&PropertyTest::PropertyCallback,
                                          base::Unretained(this), "Version"));
  WaitForCallback("Version");
  WaitForUpdates(1);

  EXPECT_EQ(20, properties_->version.value());

  // Update the value of the "Methods" property, this value should not change
  // and should not grow to contain duplicate entries.
  properties_->methods.Get(base::BindOnce(&PropertyTest::PropertyCallback,
                                          base::Unretained(this), "Methods"));
  WaitForCallback("Methods");
  WaitForUpdates(1);

  std::vector<std::string> methods = properties_->methods.value();
  ASSERT_EQ(4U, methods.size());
  EXPECT_EQ("Echo", methods[0]);
  EXPECT_EQ("SlowEcho", methods[1]);
  EXPECT_EQ("AsyncEcho", methods[2]);
  EXPECT_EQ("BrokenMethod", methods[3]);

  // Update the value of the "Objects" property, this value should not change
  // and should not grow to contain duplicate entries.
  properties_->objects.Get(base::BindOnce(&PropertyTest::PropertyCallback,
                                          base::Unretained(this), "Objects"));
  WaitForCallback("Objects");
  WaitForUpdates(1);

  std::vector<ObjectPath> objects = properties_->objects.value();
  ASSERT_EQ(1U, objects.size());
  EXPECT_EQ(ObjectPath("/TestObjectPath"), objects[0]);

  // Update the value of the "Bytes" property, this value should not change
  // and should not grow to contain duplicate entries.
  properties_->bytes.Get(base::BindOnce(&PropertyTest::PropertyCallback,
                                        base::Unretained(this), "Bytes"));
  WaitForCallback("Bytes");
  WaitForUpdates(1);

  std::vector<uint8_t> bytes = properties_->bytes.value();
  ASSERT_EQ(4U, bytes.size());
  EXPECT_EQ('T', bytes[0]);
  EXPECT_EQ('e', bytes[1]);
  EXPECT_EQ('s', bytes[2]);
  EXPECT_EQ('t', bytes[3]);
}

TEST_F(PropertyTest, Get) {
  WaitForGetAll();

  // Ask for the new Version property.
  properties_->version.Get(base::BindOnce(&PropertyTest::PropertyCallback,
                                          base::Unretained(this), "Get"));
  WaitForCallback("Get");

  // Make sure we got a property update too.
  WaitForUpdates(1);

  EXPECT_EQ(20, properties_->version.value());
}

TEST_F(PropertyTest, Set) {
  WaitForGetAll();

  // Set a new name.
  properties_->name.Set("NewService",
                        base::BindOnce(&PropertyTest::PropertyCallback,
                                       base::Unretained(this), "Set"));
  WaitForCallback("Set");

  // TestService sends a property update.
  WaitForUpdates(1);

  EXPECT_EQ("NewService", properties_->name.value());
}

TEST_F(PropertyTest, Invalidate) {
  WaitForGetAll();

  EXPECT_TRUE(properties_->name.is_valid());

  // Invalidate name.
  MethodCall method_call("org.chromium.TestInterface", "PerformAction");
  MessageWriter writer(&method_call);
  writer.AppendString("InvalidateProperty");
  writer.AppendObjectPath(ObjectPath("/org/chromium/TestService"));
  object_proxy_->CallMethod(
      &method_call, ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&PropertyTest::MethodCallback, base::Unretained(this)));
  WaitForMethodCallback();

  // TestService sends a property update.
  WaitForUpdates(1);

  EXPECT_FALSE(properties_->name.is_valid());

  // Set name to something valid.
  properties_->name.Set("NewService",
                        base::BindOnce(&PropertyTest::PropertyCallback,
                                       base::Unretained(this), "Set"));
  WaitForCallback("Set");

  // TestService sends a property update.
  WaitForUpdates(1);

  EXPECT_TRUE(properties_->name.is_valid());
}

TEST(PropertyTestStatic, ReadWriteStringMap) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  MessageWriter variant_writer(nullptr);
  MessageWriter variant_array_writer(nullptr);
  MessageWriter struct_entry_writer(nullptr);

  writer.OpenVariant("a{ss}", &variant_writer);
  variant_writer.OpenArray("{ss}", &variant_array_writer);
  const char* items[] = {"One", "Two", "Three", "Four"};
  for (unsigned i = 0; i < std::size(items); ++i) {
    variant_array_writer.OpenDictEntry(&struct_entry_writer);
    struct_entry_writer.AppendString(items[i]);
    struct_entry_writer.AppendString(base::NumberToString(i + 1));
    variant_array_writer.CloseContainer(&struct_entry_writer);
  }
  variant_writer.CloseContainer(&variant_array_writer);
  writer.CloseContainer(&variant_writer);

  MessageReader reader(message.get());
  Property<std::map<std::string, std::string>> string_map;
  EXPECT_TRUE(string_map.PopValueFromReader(&reader));
  ASSERT_EQ(4U, string_map.value().size());
  EXPECT_EQ("1", string_map.value().at("One"));
  EXPECT_EQ("2", string_map.value().at("Two"));
  EXPECT_EQ("3", string_map.value().at("Three"));
  EXPECT_EQ("4", string_map.value().at("Four"));
}

TEST(PropertyTestStatic, SerializeStringMap) {
  std::map<std::string, std::string> test_map;
  test_map["Hi"] = "There";
  test_map["Map"] = "Test";
  test_map["Random"] = "Text";

  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());

  Property<std::map<std::string, std::string>> string_map;
  string_map.ReplaceSetValueForTesting(test_map);
  string_map.AppendSetValueToWriter(&writer);

  MessageReader reader(message.get());
  EXPECT_TRUE(string_map.PopValueFromReader(&reader));
  EXPECT_EQ(test_map, string_map.value());
}

TEST(PropertyTestStatic, ReadWriteNetAddressArray) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  MessageWriter variant_writer(nullptr);
  MessageWriter variant_array_writer(nullptr);
  MessageWriter struct_entry_writer(nullptr);

  writer.OpenVariant("a(ayq)", &variant_writer);
  variant_writer.OpenArray("(ayq)", &variant_array_writer);
  uint8_t ip_bytes[] = {0x54, 0x65, 0x73, 0x74, 0x30};
  for (uint16_t i = 0; i < 5; ++i) {
    variant_array_writer.OpenStruct(&struct_entry_writer);
    ip_bytes[4] = 0x30 + i;
    struct_entry_writer.AppendArrayOfBytes(ip_bytes);
    struct_entry_writer.AppendUint16(i);
    variant_array_writer.CloseContainer(&struct_entry_writer);
  }
  variant_writer.CloseContainer(&variant_array_writer);
  writer.CloseContainer(&variant_writer);

  MessageReader reader(message.get());
  Property<std::vector<std::pair<std::vector<uint8_t>, uint16_t>>> ip_list;
  EXPECT_TRUE(ip_list.PopValueFromReader(&reader));

  ASSERT_EQ(5U, ip_list.value().size());
  size_t item_index = 0;
  for (auto& item : ip_list.value()) {
    ASSERT_EQ(5U, item.first.size());
    ip_bytes[4] = 0x30 + item_index;
    EXPECT_EQ(0, memcmp(ip_bytes, item.first.data(), 5U));
    EXPECT_EQ(item_index, item.second);
    ++item_index;
  }
}

TEST(PropertyTestStatic, SerializeNetAddressArray) {
  std::vector<std::pair<std::vector<uint8_t>, uint16_t>> test_list;

  uint8_t ip_bytes[] = {0x54, 0x65, 0x73, 0x74, 0x30};
  for (uint16_t i = 0; i < 5; ++i) {
    ip_bytes[4] = 0x30 + i;
    std::vector<uint8_t> bytes(ip_bytes, ip_bytes + std::size(ip_bytes));
    test_list.push_back(make_pair(bytes, 16));
  }

  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());

  Property<std::vector<std::pair<std::vector<uint8_t>, uint16_t>>> ip_list;
  ip_list.ReplaceSetValueForTesting(test_list);
  ip_list.AppendSetValueToWriter(&writer);

  MessageReader reader(message.get());
  EXPECT_TRUE(ip_list.PopValueFromReader(&reader));
  EXPECT_EQ(test_list, ip_list.value());
}

TEST(PropertyTestStatic, ReadWriteStringToByteVectorMapVariantWrapped) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  MessageWriter variant_writer(nullptr);
  MessageWriter dict_writer(nullptr);

  writer.OpenVariant("a{sv}", &variant_writer);
  variant_writer.OpenArray("{sv}", &dict_writer);

  const char* keys[] = {"One", "Two", "Three", "Four"};
  const std::vector<uint8_t> values[] = {{1}, {1, 2}, {1, 2, 3}, {1, 2, 3, 4}};
  for (unsigned i = 0; i < std::size(keys); ++i) {
    MessageWriter entry_writer(nullptr);
    dict_writer.OpenDictEntry(&entry_writer);

    entry_writer.AppendString(keys[i]);

    MessageWriter value_varient_writer(nullptr);
    entry_writer.OpenVariant("ay", &value_varient_writer);
    value_varient_writer.AppendArrayOfBytes(values[i]);
    entry_writer.CloseContainer(&value_varient_writer);

    dict_writer.CloseContainer(&entry_writer);
  }

  variant_writer.CloseContainer(&dict_writer);
  writer.CloseContainer(&variant_writer);

  MessageReader reader(message.get());
  Property<std::map<std::string, std::vector<uint8_t>>> test_property;
  EXPECT_TRUE(test_property.PopValueFromReader(&reader));

  ASSERT_EQ(std::size(keys), test_property.value().size());
  for (unsigned i = 0; i < std::size(keys); ++i)
    EXPECT_EQ(values[i], test_property.value().at(keys[i]));
}

TEST(PropertyTestStatic, ReadWriteStringToByteVectorMap) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  MessageWriter variant_writer(nullptr);
  MessageWriter dict_writer(nullptr);

  writer.OpenVariant("a{say}", &variant_writer);
  variant_writer.OpenArray("{say}", &dict_writer);

  const char* keys[] = {"One", "Two", "Three", "Four"};
  const std::vector<uint8_t> values[] = {{1}, {1, 2}, {1, 2, 3}, {1, 2, 3, 4}};
  for (unsigned i = 0; i < std::size(keys); ++i) {
    MessageWriter entry_writer(nullptr);
    dict_writer.OpenDictEntry(&entry_writer);

    entry_writer.AppendString(keys[i]);
    entry_writer.AppendArrayOfBytes(values[i]);

    dict_writer.CloseContainer(&entry_writer);
  }

  variant_writer.CloseContainer(&dict_writer);
  writer.CloseContainer(&variant_writer);

  MessageReader reader(message.get());
  Property<std::map<std::string, std::vector<uint8_t>>> test_property;
  EXPECT_TRUE(test_property.PopValueFromReader(&reader));

  ASSERT_EQ(std::size(keys), test_property.value().size());
  for (unsigned i = 0; i < std::size(keys); ++i)
    EXPECT_EQ(values[i], test_property.value().at(keys[i]));
}

TEST(PropertyTestStatic, SerializeStringToByteVectorMap) {
  std::map<std::string, std::vector<uint8_t>> test_map;
  test_map["Hi"] = {1, 2, 3};
  test_map["Map"] = {0xab, 0xcd};
  test_map["Random"] = {0x0};

  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());

  Property<std::map<std::string, std::vector<uint8_t>>> test_property;
  test_property.ReplaceSetValueForTesting(test_map);
  test_property.AppendSetValueToWriter(&writer);

  MessageReader reader(message.get());
  EXPECT_TRUE(test_property.PopValueFromReader(&reader));
  EXPECT_EQ(test_map, test_property.value());
}

TEST(PropertyTestStatic, ReadWriteUInt16ToByteVectorMapVariantWrapped) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  MessageWriter variant_writer(nullptr);
  MessageWriter dict_writer(nullptr);

  writer.OpenVariant("a{qv}", &variant_writer);
  variant_writer.OpenArray("{qv}", &dict_writer);

  const uint16_t keys[] = {11, 12, 13, 14};
  const std::vector<uint8_t> values[] = {{1}, {1, 2}, {1, 2, 3}, {1, 2, 3, 4}};
  for (unsigned i = 0; i < std::size(keys); ++i) {
    MessageWriter entry_writer(nullptr);
    dict_writer.OpenDictEntry(&entry_writer);

    entry_writer.AppendUint16(keys[i]);

    MessageWriter value_varient_writer(nullptr);
    entry_writer.OpenVariant("ay", &value_varient_writer);
    value_varient_writer.AppendArrayOfBytes(values[i]);
    entry_writer.CloseContainer(&value_varient_writer);

    dict_writer.CloseContainer(&entry_writer);
  }

  variant_writer.CloseContainer(&dict_writer);
  writer.CloseContainer(&variant_writer);

  MessageReader reader(message.get());
  Property<std::map<uint16_t, std::vector<uint8_t>>> test_property;
  EXPECT_TRUE(test_property.PopValueFromReader(&reader));

  ASSERT_EQ(std::size(keys), test_property.value().size());
  for (unsigned i = 0; i < std::size(keys); ++i)
    EXPECT_EQ(values[i], test_property.value().at(keys[i]));
}

TEST(PropertyTestStatic, ReadWriteUInt16ToByteVectorMap) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  MessageWriter variant_writer(nullptr);
  MessageWriter dict_writer(nullptr);

  writer.OpenVariant("a{qay}", &variant_writer);
  variant_writer.OpenArray("{qay}", &dict_writer);

  const uint16_t keys[] = {11, 12, 13, 14};
  const std::vector<uint8_t> values[] = {{1}, {1, 2}, {1, 2, 3}, {1, 2, 3, 4}};
  for (unsigned i = 0; i < std::size(keys); ++i) {
    MessageWriter entry_writer(nullptr);
    dict_writer.OpenDictEntry(&entry_writer);

    entry_writer.AppendUint16(keys[i]);
    entry_writer.AppendArrayOfBytes(values[i]);

    dict_writer.CloseContainer(&entry_writer);
  }

  variant_writer.CloseContainer(&dict_writer);
  writer.CloseContainer(&variant_writer);

  MessageReader reader(message.get());
  Property<std::map<uint16_t, std::vector<uint8_t>>> test_property;
  EXPECT_TRUE(test_property.PopValueFromReader(&reader));

  ASSERT_EQ(std::size(keys), test_property.value().size());
  for (unsigned i = 0; i < std::size(keys); ++i)
    EXPECT_EQ(values[i], test_property.value().at(keys[i]));
}

TEST(PropertyTestStatic, SerializeUInt16ToByteVectorMap) {
  std::map<uint16_t, std::vector<uint8_t>> test_map;
  test_map[11] = {1, 2, 3};
  test_map[12] = {0xab, 0xcd};
  test_map[13] = {0x0};

  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());

  Property<std::map<uint16_t, std::vector<uint8_t>>> test_property;
  test_property.ReplaceSetValueForTesting(test_map);
  test_property.AppendSetValueToWriter(&writer);

  MessageReader reader(message.get());
  EXPECT_TRUE(test_property.PopValueFromReader(&reader));
  EXPECT_EQ(test_map, test_property.value());
}

}  // namespace dbus
