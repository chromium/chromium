// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/login_session_manager.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_DBus_Properties.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_login1_Manager.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_login1_Session.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"

namespace remoting {

namespace {

using gvariant::GVariantRef;
using gvariant::ObjectPath;
using gvariant::ObjectPathCStr;

constexpr char kDbusName[] = "org.freedesktop.login1";
constexpr ObjectPathCStr kDbusPath = "/org/freedesktop/login1";

template <typename T>
base::expected<T, Loggable> GetProperty(GVariantRef<"a{sv}"> properties,
                                        const char* property_name) {
  auto property_variant = properties.LookUp(property_name);
  if (!property_variant.has_value()) {
    return base::unexpected(Loggable(
        FROM_HERE, base::StrCat({"Property not found: ", property_name})));
  }

  auto unboxed_property = property_variant->TryInto<gvariant::Boxed<T>>();
  if (!unboxed_property.has_value()) {
    return base::unexpected(unboxed_property.error());
  }
  return base::ok(unboxed_property->value);
}

}  // namespace

LoginSessionManager::SessionInfo::SessionInfo() = default;
LoginSessionManager::SessionInfo::SessionInfo(SessionInfo&&) = default;
LoginSessionManager::SessionInfo::SessionInfo(const SessionInfo&) = default;
LoginSessionManager::SessionInfo& LoginSessionManager::SessionInfo::operator=(
    SessionInfo&&) = default;
LoginSessionManager::SessionInfo& LoginSessionManager::SessionInfo::operator=(
    const SessionInfo&) = default;
LoginSessionManager::SessionInfo::~SessionInfo() = default;

LoginSessionManager::LoginSessionManager(GDBusConnectionRef connection)
    : connection_(connection) {
  DCHECK(connection_.is_initialized());
}

LoginSessionManager::~LoginSessionManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void LoginSessionManager::GetSessionInfo(const std::string& session_id,
                                         GetSessionInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  connection_.Call<org_freedesktop_login1_Manager::GetSession>(
      kDbusName, kDbusPath, std::make_tuple(session_id),
      base::BindOnce(&LoginSessionManager::OnGetSessionPathResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LoginSessionManager::GetSessionInfoByPath(
    const gvariant::ObjectPath& session_object_path,
    GetSessionInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  connection_.Call<org_freedesktop_DBus_Properties::GetAll>(
      kDbusName, session_object_path,
      std::make_tuple(org_freedesktop_login1_Session::Id::kInterfaceName),
      base::BindOnce(&LoginSessionManager::OnGetSessionPropertiesResult,
                     weak_ptr_factory_.GetWeakPtr(), session_object_path,
                     std::move(callback)));
}

void LoginSessionManager::ListUserSessions(std::string_view username,
                                           ListSessionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  connection_.Call<org_freedesktop_login1_Manager::ListSessions>(
      kDbusName, kDbusPath, std::tuple(),
      base::BindOnce(&LoginSessionManager::OnListSessionsResult,
                     weak_ptr_factory_.GetWeakPtr(), std::string(username),
                     std::move(callback)));
}

std::unique_ptr<GDBusConnectionRef::SignalSubscription>
LoginSessionManager::SubscribeSessionRemoved(SessionRemovedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return connection_
      .SignalSubscribe<org_freedesktop_login1_Manager::SessionRemoved>(
          kDbusName, kDbusPath,
          base::BindRepeating(
              [](const SessionRemovedCallback& callback,
                 std::tuple<std::string, gvariant::ObjectPath> signal_args) {
                auto [session_id, object_path] = std::move(signal_args);
                callback.Run(std::move(session_id), std::move(object_path));
              },
              std::move(callback)));
}

void LoginSessionManager::OnGetSessionPathResult(
    GetSessionInfoCallback callback,
    base::expected<std::tuple<gvariant::ObjectPath>, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(result.error()));
    return;
  }

  auto [session_path] = *result;
  GetSessionInfoByPath(session_path, std::move(callback));
}

void LoginSessionManager::OnGetSessionPropertiesResult(
    ObjectPath session_path,
    GetSessionInfoCallback callback,
    base::expected<std::tuple<GVariantRef<"a{sv}">>, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(result.error()));
    return;
  }

  auto [properties] = *result;
  SessionInfo info;
  info.object_path = std::move(session_path);

  // See:
  // https://man7.org/linux/man-pages/man5/org.freedesktop.login1.5.html#SESSION_OBJECTS

  auto session_id = GetProperty<std::string>(
      properties, org_freedesktop_login1_Session::Id::kPropertyName);
  if (!session_id.has_value()) {
    std::move(callback).Run(base::unexpected(session_id.error()));
    return;
  }
  info.session_id = session_id.value();

  auto session_class = GetProperty<std::string>(
      properties, org_freedesktop_login1_Session::Class::kPropertyName);
  if (!session_class.has_value()) {
    std::move(callback).Run(base::unexpected(session_class.error()));
    return;
  }
  info.session_class = session_class.value();

  auto session_type = GetProperty<std::string>(
      properties, org_freedesktop_login1_Session::Type::kPropertyName);
  if (!session_type.has_value()) {
    std::move(callback).Run(base::unexpected(session_type.error()));
    return;
  }
  info.session_type = session_type.value();

  auto username = GetProperty<std::string>(
      properties, org_freedesktop_login1_Session::Name::kPropertyName);
  if (!username.has_value()) {
    std::move(callback).Run(base::unexpected(username.error()));
    return;
  }
  info.username = username.value();

  auto user = GetProperty<std::tuple<uint32_t, ObjectPath>>(
      properties, org_freedesktop_login1_Session::User::kPropertyName);
  if (!user.has_value()) {
    std::move(callback).Run(base::unexpected(user.error()));
    return;
  }
  info.uid = std::get<0>(user.value());

  auto is_remote = GetProperty<bool>(
      properties, org_freedesktop_login1_Session::Remote::kPropertyName);
  if (!is_remote.has_value()) {
    std::move(callback).Run(base::unexpected(is_remote.error()));
    return;
  }
  info.is_remote = is_remote.value();

  auto service = GetProperty<std::string>(
      properties, org_freedesktop_login1_Session::Service::kPropertyName);
  if (!service.has_value()) {
    std::move(callback).Run(base::unexpected(service.error()));
    return;
  }
  info.service = std::move(*service);

  auto state = GetProperty<std::string>(
      properties, org_freedesktop_login1_Session::State::kPropertyName);
  if (!state.has_value()) {
    std::move(callback).Run(base::unexpected(state.error()));
    return;
  }
  info.state = std::move(*state);

  std::move(callback).Run(base::ok(std::move(info)));
}

void LoginSessionManager::OnListSessionsResult(
    std::string username,
    ListSessionsCallback callback,
    base::expected<std::tuple<std::vector<SessionTuple>>, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(result.error()));
    return;
  }

  auto [sessions] = std::move(*result);
  std::vector<gvariant::ObjectPath> matching_paths;
  for (auto& [session_id, uid, user_name, seat_id, object_path] : sessions) {
    if (user_name == username) {
      matching_paths.push_back(std::move(object_path));
    }
  }

  if (matching_paths.empty()) {
    std::move(callback).Run(base::ok(std::vector<SessionInfo>{}));
    return;
  }

  auto barrier = base::BarrierCallback<base::expected<SessionInfo, Loggable>>(
      matching_paths.size(),
      base::BindOnce(
          [](ListSessionsCallback callback,
             std::vector<base::expected<SessionInfo, Loggable>> results) {
            std::vector<SessionInfo> session_infos;
            session_infos.reserve(results.size());
            for (auto& session_result : results) {
              if (session_result.has_value()) {
                session_infos.push_back(std::move(*session_result));
              } else {
                LOG(WARNING) << "Failed to get properties for session: "
                             << session_result.error();
              }
            }
            std::move(callback).Run(base::ok(std::move(session_infos)));
          },
          std::move(callback)));

  for (const auto& path : matching_paths) {
    GetSessionInfoByPath(path, barrier);
  }
}

void LoginSessionManager::TerminateSession(
    const gvariant::ObjectPath& session_object_path,
    TerminateSessionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  connection_.Call<org_freedesktop_login1_Session::Terminate>(
      kDbusName, session_object_path, std::tuple(),
      base::BindOnce(&LoginSessionManager::OnTerminateSessionResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LoginSessionManager::OnTerminateSessionResult(
    TerminateSessionCallback callback,
    base::expected<std::tuple<>, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(result.error()));
    return;
  }
  std::move(callback).Run(base::ok());
}

}  // namespace remoting
