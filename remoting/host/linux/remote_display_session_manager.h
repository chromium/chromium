// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_REMOTE_DISPLAY_SESSION_MANAGER_H_
#define REMOTING_HOST_LINUX_REMOTE_DISPLAY_SESSION_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "remoting/host/linux/gdm_remote_display_manager.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/login_session_manager.h"
#include "remoting/host/linux/login_session_reporter_server.h"
#include "remoting/host/linux/passwd_utils.h"
#include "remoting/host/mojom/login_session.mojom-forward.h"

namespace remoting {

// Class to create or terminate GDM remote displays and get information about
// the remote display's current systemd login session.
//
// GDM creates multiple remote displays with the same remote ID but different
// session IDs. To avoid confusions, "remote displays" in this class refers to
// a collection of GDM remote displays with the same session ID, while the
// actual GDM remote displays will be referred to as "remote display sessions".
//
// This class requires current process to be run as root.
class RemoteDisplaySessionManager : public GdmRemoteDisplayManager::Observer,
                                    public mojom::LoginSessionObserver {
 public:
  using Callback = base::OnceCallback<void(base::expected<void, Loggable>)>;

  struct RemoteDisplaySession {
    RemoteDisplaySession();
    RemoteDisplaySession(RemoteDisplaySession&&);
    RemoteDisplaySession(const RemoteDisplaySession&);
    ~RemoteDisplaySession();

    RemoteDisplaySession& operator=(RemoteDisplaySession&&);
    RemoteDisplaySession& operator=(const RemoteDisplaySession&);

    // Information about the remote display's current systemd login session.
    // This is null if no session has been created for the remote display yet.
    std::optional<LoginSessionManager::SessionInfo> session_info;

    // Information about the remote display's user. This is null if no session
    // has been created for the remote display yet.
    std::optional<PasswdUserInfo> user_info;

    // Environment variables for launching processes under the remote display's
    // current systemd login session. Empty if the session is not ready yet.
    // Note that it is possible that `session_info` has value while this map is
    // empty, in which case you should wait for OnRemoteDisplayChanged() to be
    // called.
    base::EnvironmentMap environment_variables;
  };

  struct RemoteDisplayInfo {
    RemoteDisplayInfo();
    RemoteDisplayInfo(RemoteDisplayInfo&&);
    RemoteDisplayInfo(const RemoteDisplayInfo&);
    ~RemoteDisplayInfo();

    RemoteDisplayInfo& operator=(RemoteDisplayInfo&&);
    RemoteDisplayInfo& operator=(const RemoteDisplayInfo&);

    base::flat_map<gvariant::ObjectPath /*GDM remote display object path*/,
                   RemoteDisplaySession>
        sessions;
  };

  using RemoteDisplayMap =
      base::flat_map<std::string /*display_name*/, RemoteDisplayInfo>;

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called whenever the remote display sessions have changed and are ready
    // to use (all fields are populated in `info`).
    virtual void OnRemoteDisplayChanged(std::string_view display_name,
                                        const RemoteDisplayInfo& info) {}

    // Called whenever a remote display is terminated, i.e. all remote display
    // sessions have been terminated.
    virtual void OnRemoteDisplayTerminated(std::string_view display_name) {}
  };

  RemoteDisplaySessionManager();
  ~RemoteDisplaySessionManager() override;

  RemoteDisplaySessionManager(const RemoteDisplaySessionManager&) = delete;
  RemoteDisplaySessionManager& operator=(const RemoteDisplaySessionManager&) =
      delete;

  // Starts the manager. Must be called exactly once before calling other
  // methods. `callback` is called once the manager has successfully started or
  // failed to start.
  void Start(Delegate* delegate, Callback callback);

  // Creates a new remote display. OnRemoteDisplayChanged() will be
  // called once it is created and its associated sessions are ready to use.
  // `display_name` is a unique string to identify the remote display.
  void CreateRemoteDisplay(std::string_view display_name, Callback callback);

  // Terminates a remote display and all its associated remote display sessions.
  // OnRemoteDisplayTerminated() will be called once the remote display is
  // terminated. You cannot terminate a remote display without an associated
  // session.
  void TerminateRemoteDisplay(std::string_view display_name, Callback callback);

  // Terminates a specific remote display session and calls the callback once
  // it is done. OnRemoteDisplayTerminated() will be called if all remote
  // display sessions of a remote display have been terminated.
  void TerminateRemoteDisplaySession(const RemoteDisplaySession& session,
                                     Callback callback);

  // Gets the remote display info for `display_name`. Returns nullptr if the
  // remote display doesn't exist.
  const RemoteDisplayInfo* GetRemoteDisplayInfo(std::string_view display_name);

  const RemoteDisplayMap& remote_displays() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return remote_displays_;
  }

 private:
  enum class StartState {
    NOT_STARTED,
    STARTING,
    STARTED,
  };

  void QuerySessionInfo(const std::string& display_name,
                        const gvariant::ObjectPath& display_path,
                        const std::string& session_id);
  void PopulateSessionEnvironment(
      const std::string& display_name,
      const RemoteDisplayInfo& display_info,
      RemoteDisplaySession& session,
      mojom::LoginSessionInfoPtr session_reporter_info);

  // If `start_state_` is `STARTING` and there are no more session info queries
  // blocking startup, then transition to `STARTED` and run the init callback.
  void HandleSessionInfoQueriesBlockingStartup();

  void OnCreateDbusConnectionResult(
      base::expected<GDBusConnectionRef, Loggable> result);
  void OnGdmRemoteDisplayManagerStarted(base::expected<void, Loggable> result);

  // GdmRemoteDisplayManager::Observer:
  // Note: GDM will create multiple remote displays with the same remote ID but
  // different session IDs, so they are actually RemoteDisplaySessions in the
  // context of this class.
  void OnRemoteDisplayCreated(
      const gvariant::ObjectPath& display_path,
      const GdmRemoteDisplayManager::RemoteDisplay& display) override;
  void OnRemoteDisplayRemoved(const gvariant::ObjectPath& display_path,
                              const gvariant::ObjectPath& remote_id) override;
  void OnRemoteDisplayChanged(
      const gvariant::ObjectPath& display_path,
      const GdmRemoteDisplayManager::RemoteDisplay& display) override;

  // mojom::LoginSessionObserver:
  void OnLoginSessionCreated(mojom::LoginSessionInfoPtr session_info) override;

  void OnSessionInfoReady(
      const std::string& display_name,
      const gvariant::ObjectPath& display_path,
      base::expected<LoginSessionManager::SessionInfo, Loggable> result);

  void FetchSystemdEnvironmentVariables(
      const std::string& display_name,
      const gvariant::ObjectPath& display_path,
      const std::string& username);

  void OnGetUserSystemdEnvironmentResult(
      const std::string& display_name,
      const gvariant::ObjectPath& display_path,
      const std::string& output);

  SEQUENCE_CHECKER(sequence_checker_);

  StartState start_state_ GUARDED_BY_CONTEXT(sequence_checker_) =
      StartState::NOT_STARTED;
  Callback init_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<Delegate> delegate_ GUARDED_BY_CONTEXT(sequence_checker_);
  GdmRemoteDisplayManager remote_display_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<LoginSessionManager> login_session_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);
  LoginSessionReporterServer login_session_reporter_server_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
  GDBusConnectionRef connection_ GUARDED_BY_CONTEXT(sequence_checker_);
  RemoteDisplayMap remote_displays_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Stores information from the session reporter process if received before the
  // information from the systemd D-BUS call.
  // TODO: crbug.com/488713023 - remove this when we poll systemd user
  // environments for GNOME 49.
  base::flat_map<std::string /*session_id*/, mojom::LoginSessionInfoPtr>
      pending_session_reporter_info_;

  // Tracks remote display sessions awaiting systemd session info which blocks
  // the startup process.
  // If there are remote display sessions that exist before Start() was called,
  // then the start callback won't be called until the `session_info` of all of
  // these sessions have been populated. This allows the caller to terminate all
  // CRD-managed remote displays that were leaked from the previous CRD host
  // incarnation.
  // TODO: crbug.com/488713023 - remove this when we poll systemd user
  // environments for GNOME 49.
  base::flat_set<gvariant::ObjectPath /*display_path*/>
      session_info_queries_blocking_startup_
          GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<RemoteDisplaySessionManager> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_REMOTE_DISPLAY_SESSION_MANAGER_H_
