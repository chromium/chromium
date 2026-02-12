// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GDM_REMOTE_DISPLAY_MANAGER_H_
#define REMOTING_HOST_LINUX_GDM_REMOTE_DISPLAY_MANAGER_H_

#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"

namespace remoting {

// Class to create or observe GDM remote displays.
// See: https://blogs.gnome.org/joantolo/2025/12/23/remote-login-design/
//
// This class requires current process to be run as root. It can be constructed
// on one sequence, then initialized, used, and destructed entirely on another
// sequence.
class GdmRemoteDisplayManager {
 public:
  // A struct that encapsulates the org.gnome.DisplayManager.RemoteDisplay
  // interface.
  struct RemoteDisplay {
    // The remote ID provided when CreateRemoteDisplay() is called.
    gvariant::ObjectPath remote_id;

    // The current session ID. Note that this will be an empty string if no
    // login session has been created for the remote display yet, in which case
    // you should wait for OnRemoteDisplaySessionChanged() to be called.
    std::string session_id;
  };

  using Callback = base::OnceCallback<void(base::expected<void, Loggable>)>;

  // `display_path`: a D-Bus object path in /org/gnome/DisplayManager/Displays
  // that points to the RemoteDisplay object.
  using RemoteDisplayMap =
      base::flat_map<gvariant::ObjectPath /* display_path */, RemoteDisplay>;

  // Interface for observing changes of GDM remote displays.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when a new remote display has been created.
    virtual void OnRemoteDisplayCreated(
        const gvariant::ObjectPath& display_path,
        const RemoteDisplay& display) {}

    // Called when an existing remote display has been removed.
    virtual void OnRemoteDisplayRemoved(
        const gvariant::ObjectPath& display_path,
        const gvariant::ObjectPath& remote_id) {}

    // Called when a remote display's session ID has changed, which usually
    // happens when the user logs in from the GDM greeter session.
    virtual void OnRemoteDisplaySessionChanged(
        const gvariant::ObjectPath& display_path,
        const RemoteDisplay& display) {}
  };

  GdmRemoteDisplayManager();
  ~GdmRemoteDisplayManager();

  GdmRemoteDisplayManager(const GdmRemoteDisplayManager&) = delete;
  GdmRemoteDisplayManager& operator=(const GdmRemoteDisplayManager&) = delete;

  // Initializes the remote display manager and `callback` once the
  // initialization has succeeded or failed. All methods below this method must
  // be called after `callback` is called without an error.
  // `observer` must outlive `this`.
  // `connection` must be an initialized system bus connection.
  void Init(GDBusConnectionRef connection,
            Observer* observer,
            Callback callback);

  // Creates a new GDM remote display with the given `remote_id`. Note that
  // while `remote_id` is an object path, it is only used as a unique
  // identifier; no objects will be created at the given path.
  // Calls `callback` once the async request has succeeded or failed. Note that
  // this does not indicate that a remote display itself has already been
  // created, which is what `Observer::OnRemoteDisplayCreated()` is used for, so
  // `callback` is mostly used in case an error has occurred.
  void CreateRemoteDisplay(gvariant::ObjectPath remote_id, Callback callback);

  // TODO: crbug.com/465193343 - See if we need a RemoveRemoteDisplay() method.
  // GDM does not have a D-Bus method for doing so, but the remote display will
  // be removed if you terminate the remote display's login session.

  // Returns all GDM remote displays.
  const RemoteDisplayMap& remote_displays() const { return remote_displays_; }

 private:
  enum class InitializationState {
    NOT_INITIALIZED,
    INITIALIZING,
    INITIALIZED,
  };

  void SubscribeSignals();
  void AddRemoteDisplay(
      const gvariant::ObjectPath& display_path,
      gvariant::GVariantRef<"a{sa{sv}}"> interfaces_and_properties);

  void OnGetAllRemoteDisplaysResult(
      Callback init_callback,
      base::expected<std::tuple<gvariant::GVariantRef<"a{oa{sa{sv}}}">>,
                     Loggable> result);
  void OnCreateRemoteDisplayResult(
      Callback callback,
      base::expected<std::tuple<>, Loggable> result);
  void OnInterfacesAddedInternal(
      const gvariant::ObjectPath& display_path,
      gvariant::GVariantRef<"a{sa{sv}}"> interfaces_and_properties,
      bool notify_observer);
  void OnInterfacesAdded(std::tuple<gvariant::ObjectPath,
                                    gvariant::GVariantRef<"a{sa{sv}}">> args);
  void OnInterfacesRemoved(
      std::tuple<gvariant::ObjectPath, std::vector<std::string>> args);
  void OnRemoteDisplayPropertyChanged(
      const gvariant::ObjectPath& display_path,
      std::tuple<gvariant::GVariantRef<"s">,
                 gvariant::GVariantRef<"a{sv}">,
                 std::vector<std::string>> args);

  SEQUENCE_CHECKER(sequence_checker_);

  InitializationState initialization_state_ GUARDED_BY_CONTEXT(
      sequence_checker_) = InitializationState::NOT_INITIALIZED;
  raw_ptr<Observer> observer_ GUARDED_BY_CONTEXT(sequence_checker_);
  GDBusConnectionRef connection_ GUARDED_BY_CONTEXT(sequence_checker_);
  RemoteDisplayMap remote_displays_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<GDBusConnectionRef::SignalSubscription>
      interfaces_added_subscription_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<GDBusConnectionRef::SignalSubscription>
      interfaces_removed_subscription_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<GDBusConnectionRef::SignalSubscription>
      remote_display_property_subscription_
          GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<GdmRemoteDisplayManager> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GDM_REMOTE_DISPLAY_MANAGER_H_
