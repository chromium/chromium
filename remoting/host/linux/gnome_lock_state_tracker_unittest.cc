// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_lock_state_tracker.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "remoting/base/loggable.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_DBus_Properties.h"
#include "remoting/host/linux/dbus_interfaces/org_gnome_Mutter_RemoteDesktop.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr gvariant::ObjectPathCStr kSessionPath =
    "/org/gnome/Mutter/RemoteDesktop/Session/Dummy";

class MockDbusService {
 public:
  MockDbusService() {
    dbus_thread_ = std::make_unique<base::Thread>("D-Bus Client Thread");
    base::Thread::Options thread_options(base::MessagePumpType::IO, 0);
    bool thread_started =
        dbus_thread_->StartWithOptions(std::move(thread_options));
    DCHECK(thread_started);
  }

  ~MockDbusService() {
    if (bus_) {
      dbus_thread_->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, bus_));
    }
    dbus_thread_->Stop();
  }

  void Initialize(const std::string& service_name,
                  const std::string& object_path) {
    base::test::TestFuture<void> init_future;
    dbus_thread_->task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&MockDbusService::InitializeOnThread,
                       base::Unretained(this), service_name, object_path),
        init_future.GetCallback());
    init_future.Get();
  }

  void SetState(bool caps_lock, bool num_lock) {
    caps_lock_state_ = caps_lock;
    num_lock_state_ = num_lock;
  }

  void SendPropertiesChanged(bool caps_lock, bool num_lock) {
    dbus_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockDbusService::SendPropertiesChangedOnThread,
                       base::Unretained(this), caps_lock, num_lock));
  }

 private:
  void InitializeOnThread(const std::string& service_name,
                          const std::string& object_path) {
    dbus::Bus::Options bus_options;
    bus_options.bus_type = dbus::Bus::SESSION;
    bus_options.connection_type = dbus::Bus::PRIVATE;
    bus_options.dbus_task_runner = dbus_thread_->task_runner();
    bus_ = new dbus::Bus(std::move(bus_options));

    bool connected = bus_->Connect();
    DCHECK(connected);

    bool owned = bus_->RequestOwnershipAndBlock(service_name,
                                                dbus::Bus::REQUIRE_PRIMARY);
    DCHECK(owned);

    exported_object_ =
        bus_->GetExportedObject(dbus::ObjectPath(object_path.c_str()));
    exported_object_->ExportMethod(
        "org.freedesktop.DBus.Properties", "Get",
        base::BindRepeating(&MockDbusService::OnGetProperty,
                            base::Unretained(this)),
        base::BindOnce([](const std::string& interface_name,
                          const std::string& method_name,
                          bool success) { DCHECK(success); }));
  }

  void OnGetProperty(dbus::MethodCall* method_call,
                     dbus::ExportedObject::ResponseSender response_sender) {
    dbus::MessageReader reader(method_call);
    std::string interface_name;
    std::string property_name;
    if (reader.PopString(&interface_name) && reader.PopString(&property_name)) {
      if (interface_name == "org.gnome.Mutter.RemoteDesktop.Session") {
        if (property_name == "CapsLockState") {
          std::unique_ptr<dbus::Response> response =
              dbus::Response::FromMethodCall(method_call);
          dbus::MessageWriter writer(response.get());
          writer.AppendVariantOfBool(caps_lock_state_);
          std::move(response_sender).Run(std::move(response));
          return;
        }
        if (property_name == "NumLockState") {
          std::unique_ptr<dbus::Response> response =
              dbus::Response::FromMethodCall(method_call);
          dbus::MessageWriter writer(response.get());
          writer.AppendVariantOfBool(num_lock_state_);
          std::move(response_sender).Run(std::move(response));
          return;
        }
      }
    }
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, "org.freedesktop.DBus.Error.Failed",
            "Property not supported"));
  }

  void SendPropertiesChangedOnThread(bool caps_lock, bool num_lock) {
    dbus::Signal signal("org.freedesktop.DBus.Properties", "PropertiesChanged");
    dbus::MessageWriter writer(&signal);
    writer.AppendString("org.gnome.Mutter.RemoteDesktop.Session");

    dbus::MessageWriter dict_writer(nullptr);
    writer.OpenArray("{sv}", &dict_writer);

    dbus::MessageWriter entry_writer1(nullptr);
    dict_writer.OpenDictEntry(&entry_writer1);
    entry_writer1.AppendString("CapsLockState");
    entry_writer1.AppendVariantOfBool(caps_lock);
    dict_writer.CloseContainer(&entry_writer1);

    dbus::MessageWriter entry_writer2(nullptr);
    dict_writer.OpenDictEntry(&entry_writer2);
    entry_writer2.AppendString("NumLockState");
    entry_writer2.AppendVariantOfBool(num_lock);
    dict_writer.CloseContainer(&entry_writer2);

    writer.CloseContainer(&dict_writer);

    dbus::MessageWriter invalidated_writer(nullptr);
    writer.OpenArray("s", &invalidated_writer);
    writer.CloseContainer(&invalidated_writer);

    exported_object_->SendSignal(&signal);
  }

  std::unique_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> bus_;
  scoped_refptr<dbus::ExportedObject> exported_object_;

  bool caps_lock_state_ = false;
  bool num_lock_state_ = false;
};

}  // namespace

class GnomeLockStateTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    std::string uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    std::erase(uuid, '-');
    service_name_ = "org.gnome.Mutter.RemoteDesktop.Test.u" + uuid;
    // Start the mock service on a background thread.
    mock_service_ = std::make_unique<MockDbusService>();
    mock_service_->Initialize(service_name_,
                              kSessionPath.c_str());

    // Create the GDBusConnectionRef for the tracker.
    base::test::TestFuture<base::expected<GDBusConnectionRef, Loggable>>
        connection_future;
    GDBusConnectionRef::CreateForSessionBus(connection_future.GetCallback());
    ASSERT_TRUE(connection_future.Get().has_value());
    connection_ = connection_future.Take().value();
  }

 protected:
  void WaitForCapsLockState(const GnomeLockStateTracker& tracker,
                            bool expected) {
    base::test::TestFuture<void> future;
    base::RepeatingTimer timer;
    timer.Start(FROM_HERE, base::Milliseconds(10),
                base::BindLambdaForTesting([&]() {
                  if (tracker.GetCapsLockState() == expected) {
                    future.SetValue();
                  }
                }));
    ASSERT_TRUE(future.Wait());
  }

  void WaitForNumLockState(const GnomeLockStateTracker& tracker,
                           bool expected) {
    base::test::TestFuture<void> future;
    base::RepeatingTimer timer;
    timer.Start(FROM_HERE, base::Milliseconds(10),
                base::BindLambdaForTesting([&]() {
                  if (tracker.GetNumLockState() == expected) {
                    future.SetValue();
                  }
                }));
    ASSERT_TRUE(future.Wait());
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};

  std::string service_name_;
  std::unique_ptr<MockDbusService> mock_service_;
  GDBusConnectionRef connection_;
};

TEST_F(GnomeLockStateTrackerTest, InitialStateAndPropertyChanges) {
  mock_service_->SetState(/*caps_lock=*/true, /*num_lock=*/true);

  GnomeLockStateTracker tracker(connection_, gvariant::ObjectPath(kSessionPath),
                                service_name_.c_str());
  tracker.Start();

  WaitForCapsLockState(tracker, true);
  WaitForNumLockState(tracker, true);

  EXPECT_TRUE(tracker.GetCapsLockState());
  EXPECT_TRUE(tracker.GetNumLockState());

  // Test setting expected states locally.
  tracker.SetExpectedCapsLockState(false);
  tracker.SetExpectedNumLockState(false);
  EXPECT_FALSE(tracker.GetCapsLockState());
  EXPECT_FALSE(tracker.GetNumLockState());

  // Test PropertiesChanged signal propagation.
  mock_service_->SendPropertiesChanged(/*caps_lock=*/true, /*num_lock=*/true);
  WaitForCapsLockState(tracker, true);
  WaitForNumLockState(tracker, true);

  EXPECT_TRUE(tracker.GetCapsLockState());
  EXPECT_TRUE(tracker.GetNumLockState());
}

}  // namespace remoting
