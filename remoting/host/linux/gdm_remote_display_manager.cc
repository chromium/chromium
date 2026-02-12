// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gdm_remote_display_manager.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_DBus_ObjectManager.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_DBus_Properties.h"
#include "remoting/host/linux/dbus_interfaces/org_gnome_DisplayManager.h"
#include "remoting/host/linux/gvariant_ref.h"

namespace remoting {

namespace {

using gvariant::GVariantRef;
using gvariant::ObjectPath;
using gvariant::ObjectPathCStr;

constexpr char kGdmBusName[] = "org.gnome.DisplayManager";
constexpr ObjectPathCStr kGdmDisplaysPath =
    "/org/gnome/DisplayManager/Displays";
constexpr ObjectPathCStr kGdmRemoteDisplayFactoryPath =
    "/org/gnome/DisplayManager/RemoteDisplayFactory";

}  // namespace

GdmRemoteDisplayManager::GdmRemoteDisplayManager() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

GdmRemoteDisplayManager::~GdmRemoteDisplayManager() = default;

void GdmRemoteDisplayManager::Init(GDBusConnectionRef connection,
                                   Observer* observer,
                                   Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(initialization_state_, InitializationState::NOT_INITIALIZED);
  DCHECK(connection.is_initialized());

  connection_ = connection;
  observer_ = observer;
  initialization_state_ = InitializationState::INITIALIZING;
  SubscribeSignals();

  // Get all remote displays that have already been created before the
  // initialization.
  connection_.Call<org_freedesktop_DBus_ObjectManager::GetManagedObjects>(
      kGdmBusName, kGdmDisplaysPath, std::tuple(),
      base::BindOnce(&GdmRemoteDisplayManager::OnGetAllRemoteDisplaysResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void GdmRemoteDisplayManager::CreateRemoteDisplay(ObjectPath remote_id,
                                                  Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(initialization_state_, InitializationState::INITIALIZED);

  // Bind GdmRemoteDisplayManager::OnCreateRemoteDisplayResult so that
  // `callback` will be silently dropped if `this` is destructed before the
  // connection is created.
  connection_
      .Call<org_gnome_DisplayManager_RemoteDisplayFactory::CreateRemoteDisplay>(
          kGdmBusName, kGdmRemoteDisplayFactoryPath, std::tuple(remote_id),
          base::BindOnce(&GdmRemoteDisplayManager::OnCreateRemoteDisplayResult,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void GdmRemoteDisplayManager::SubscribeSignals() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(initialization_state_, InitializationState::INITIALIZING);

  interfaces_added_subscription_ =
      connection_
          .SignalSubscribe<org_freedesktop_DBus_ObjectManager::InterfacesAdded>(
              kGdmBusName, kGdmDisplaysPath,
              base::BindRepeating(&GdmRemoteDisplayManager::OnInterfacesAdded,
                                  weak_ptr_factory_.GetWeakPtr()));

  interfaces_removed_subscription_ = connection_.SignalSubscribe<
      org_freedesktop_DBus_ObjectManager::InterfacesRemoved>(
      kGdmBusName, kGdmDisplaysPath,
      base::BindRepeating(&GdmRemoteDisplayManager::OnInterfacesRemoved,
                          weak_ptr_factory_.GetWeakPtr()));
}

void GdmRemoteDisplayManager::OnGetAllRemoteDisplaysResult(
    Callback init_callback,
    base::expected<std::tuple<GVariantRef<"a{oa{sa{sv}}}">>, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(initialization_state_, InitializationState::INITIALIZING);

  if (!result.has_value()) {
    initialization_state_ = InitializationState::NOT_INITIALIZED;
    std::move(init_callback).Run(base::unexpected(result.error()));
    return;
  }

  initialization_state_ = InitializationState::INITIALIZED;

  auto [interfaces] = result.value();

  for (auto [display_path, interfaces_and_properties] : interfaces) {
    // Don't notify observers of the pre-existing remote displays.
    OnInterfacesAddedInternal(display_path.Into<ObjectPath>(),
                              interfaces_and_properties,
                              /*notify_observer=*/false);
  }

  std::move(init_callback).Run(base::ok());
}

void GdmRemoteDisplayManager::OnCreateRemoteDisplayResult(
    Callback callback,
    base::expected<std::tuple<>, Loggable> result) {
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(result.error()));
    return;
  }
  std::move(callback).Run(base::ok());
}

void GdmRemoteDisplayManager::OnInterfacesAddedInternal(
    const ObjectPath& display_path,
    GVariantRef<"a{sa{sv}}"> interfaces_and_properties,
    bool notify_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto properties = interfaces_and_properties.LookUp(
      org_gnome_DisplayManager_RemoteDisplay::RemoteId::kInterfaceName);
  if (!properties.has_value()) {
    return;
  }

  auto remote_id_variant = properties->LookUp(
      org_gnome_DisplayManager_RemoteDisplay::RemoteId::kPropertyName);
  if (!remote_id_variant.has_value()) {
    LOG(ERROR) << "No RemoteId property found for RemoteDisplay interface.";
    return;
  }

  auto remote_id_unboxed =
      remote_id_variant->TryInto<gvariant::Boxed<ObjectPath>>();
  if (!remote_id_unboxed.has_value()) {
    LOG(ERROR) << "Failed to get RemoteId: " << remote_id_unboxed.error();
    return;
  }
  auto remote_id = remote_id_unboxed->value;

  std::string session_id;
  auto session_id_variant = properties->LookUp(
      org_gnome_DisplayManager_RemoteDisplay::SessionId::kPropertyName);
  if (session_id_variant.has_value()) {
    auto session_id_unboxed =
        session_id_variant->TryInto<gvariant::Boxed<std::string>>();
    if (!session_id_unboxed.has_value()) {
      LOG(ERROR) << "Failed to get SessionId: " << session_id_unboxed.error();
    } else {
      session_id = session_id_unboxed->value;
    }
  }

  RemoteDisplay& new_display = remote_displays_[display_path];
  new_display.remote_id = remote_id;
  new_display.session_id = session_id;
  if (notify_observer) {
    observer_->OnRemoteDisplayCreated(display_path, new_display);
  }

  // Subscribe to property changes for this remote display.
  remote_display_property_subscription_ =
      connection_
          .SignalSubscribe<org_freedesktop_DBus_Properties::PropertiesChanged>(
              kGdmBusName, display_path,
              base::BindRepeating(
                  &GdmRemoteDisplayManager::OnRemoteDisplayPropertyChanged,
                  weak_ptr_factory_.GetWeakPtr(), display_path));
}

void GdmRemoteDisplayManager::OnInterfacesAdded(
    std::tuple<ObjectPath, GVariantRef<"a{sa{sv}}">> args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto [display_path, interfaces_and_properties] = args;
  OnInterfacesAddedInternal(display_path, interfaces_and_properties,
                            /*notify_observer=*/true);
}

void GdmRemoteDisplayManager::OnInterfacesRemoved(
    std::tuple<ObjectPath, std::vector<std::string>> args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& [object_path, interfaces] = args;
  auto it = remote_displays_.find(object_path);
  if (it != remote_displays_.end()) {
    ObjectPath remote_id = it->second.remote_id;
    remote_displays_.erase(it);
    observer_->OnRemoteDisplayRemoved(object_path, remote_id);
  }
}

void GdmRemoteDisplayManager::OnRemoteDisplayPropertyChanged(
    const ObjectPath& display_path,
    std::tuple<GVariantRef<"s">, GVariantRef<"a{sv}">, std::vector<std::string>>
        args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& [interface_name, changed_properties, invalidated_properties] = args;
  if (interface_name.string_view() !=
      org_gnome_DisplayManager_RemoteDisplay::SessionId::kInterfaceName) {
    return;
  }

  auto remote_display_it = remote_displays_.find(display_path);
  if (remote_display_it == remote_displays_.end()) {
    LOG(ERROR) << "Cannot find remote display for display path "
               << display_path.value();
    return;
  }

  auto session_id_variant = changed_properties.LookUp(
      org_gnome_DisplayManager_RemoteDisplay::SessionId::kPropertyName);
  if (session_id_variant.has_value()) {
    auto session_id_boxed =
        session_id_variant->TryInto<gvariant::Boxed<std::string>>();
    if (!session_id_boxed.has_value()) {
      LOG(ERROR) << "Failed to get SessionId: " << session_id_boxed.error();
      return;
    }
    remote_display_it->second.session_id = session_id_boxed->value;
    observer_->OnRemoteDisplaySessionChanged(display_path,
                                             remote_display_it->second);
    return;
  }

  if (std::ranges::contains(
          invalidated_properties,
          org_gnome_DisplayManager_RemoteDisplay::SessionId::kPropertyName)) {
    LOG(WARNING) << "Display " << display_path.value()
                 << " (SessionId: " << remote_display_it->second.session_id
                 << ") is no longer associated with a login session.";
    remote_display_it->second.session_id = {};
    observer_->OnRemoteDisplaySessionChanged(display_path,
                                             remote_display_it->second);
  }
}

}  // namespace remoting
