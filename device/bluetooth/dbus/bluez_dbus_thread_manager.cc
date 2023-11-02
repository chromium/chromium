// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluez_dbus_thread_manager.h"

#include <memory>

#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/threading/thread.h"
#include "dbus/bus.h"

namespace bluez {

static BluezDBusThreadManager* g_bluez_dbus_thread_manager = NULL;

BluezDBusThreadManager::BluezDBusThreadManager() {
  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  dbus_thread_ = std::make_unique<base::Thread>("Bluez D-Bus thread");
  dbus_thread_->StartWithOptions(std::move(thread_options));

  // Create the connection to the system bus.
  dbus::Bus::Options system_bus_options;
  system_bus_options.bus_type = dbus::Bus::SYSTEM;
  system_bus_options.connection_type = dbus::Bus::PRIVATE;
  system_bus_options.dbus_task_runner = dbus_thread_->task_runner();
  system_bus_ = new dbus::Bus(system_bus_options);
}

BluezDBusThreadManager::~BluezDBusThreadManager() {
  // Shut down the bus. During the browser shutdown, it's ok to shut down
  // the bus synchronously.
  if (system_bus_.get())
    system_bus_->ShutdownOnDBusThreadAndBlock();

  // Stop the D-Bus thread.
  if (dbus_thread_)
    dbus_thread_->Stop();

  if (!g_bluez_dbus_thread_manager)
    return;  // Called form Shutdown() or local test instance.

  // There should never be both a global instance and a local instance.
  CHECK(this == g_bluez_dbus_thread_manager);
}

dbus::Bus* BluezDBusThreadManager::GetSystemBus() {
  return system_bus_.get();
}

// static
void BluezDBusThreadManager::Initialize() {
  CHECK(!g_bluez_dbus_thread_manager);
  g_bluez_dbus_thread_manager = new BluezDBusThreadManager();
}

// static
void BluezDBusThreadManager::Shutdown() {
  // Ensure that we only shutdown BluezDBusThreadManager once.
  CHECK(g_bluez_dbus_thread_manager);
  BluezDBusThreadManager* dbus_thread_manager = g_bluez_dbus_thread_manager;
  g_bluez_dbus_thread_manager = nullptr;
  delete dbus_thread_manager;
  DVLOG(1) << "BluezDBusThreadManager Shutdown completed";
}

// static
BluezDBusThreadManager* BluezDBusThreadManager::Get() {
  CHECK(g_bluez_dbus_thread_manager)
      << "BluezDBusThreadManager::Get() called before Initialize()";
  return g_bluez_dbus_thread_manager;
}

}  // namespace bluez
