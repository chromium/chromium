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
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "remoting/host/linux/gdm_remote_display_manager.h"
#include "remoting/host/linux/login_session_manager.h"
#include "remoting/host/linux/login_session_reporter_server.h"
#include "remoting/host/linux/passwd_utils.h"
#include "remoting/host/mojom/login_session.mojom-forward.h"

namespace remoting {

// Class to create or terminate GDM remote displays and get information about
// the remote display's current systemd login session.
//
// This class requires current process to be run as root.
class RemoteDisplaySessionManager : public GdmRemoteDisplayManager::Observer,
                                    public mojom::LoginSessionObserver {
 public:
  using Callback = base::OnceCallback<void(base::expected<void, Loggable>)>;

  struct RemoteDisplayInfo {
    RemoteDisplayInfo();
    RemoteDisplayInfo(RemoteDisplayInfo&&);
    RemoteDisplayInfo(const RemoteDisplayInfo&);
    ~RemoteDisplayInfo();

    RemoteDisplayInfo& operator=(RemoteDisplayInfo&&);
    RemoteDisplayInfo& operator=(const RemoteDisplayInfo&);

    // Information about the remote display's current systemd login session.
    // This is null if no session has been created for the remote display yet.
    std::optional<LoginSessionManager::SessionInfo> session_info;

    // Information about the remote display's user. This is null if no session
    // has been created for the remote display yet.
    std::optional<PasswdUserInfo> user_info;

    // Environment variables for launching processes under the remote display's
    // current systemd login session. Empty if the session is not ready yet.
    // Note that it is possible that `session_info` has value while this map is
    // empty, in which case you should wait for OnRemoteDisplaySessionChanged()
    // to be called.
    base::EnvironmentMap environment_variables;
  };

  using RemoteDisplayMap =
      base::flat_map<std::string /*display_name*/, RemoteDisplayInfo>;

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called whenever the remote display's session has changed and is ready to
    // use (all fields are populated in `info`), or no session is associated
    // with the remote display any more.
    virtual void OnRemoteDisplaySessionChanged(std::string_view display_name,
                                               const RemoteDisplayInfo& info) {}

    // Called whenever a remote display is terminated.
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

  // Creates a new remote display. OnRemoteDisplaySessionChanged() will be
  // called once it is created and its associated session is ready to use.
  // `display_name` is a unique string to identify the remote display.
  void CreateRemoteDisplay(std::string_view display_name, Callback callback);

  // Terminates a remote display. This is done by terminating a remote display's
  // associated session. OnRemoteDisplayTerminated() will be called once the
  // remote display is terminated. You cannot terminate a remote display without
  // an associated session.
  void TerminateRemoteDisplay(std::string_view display_name, Callback callback);

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
                        const std::string& session_id);
  void PopulateSessionEnvironment(
      const std::string& display_name,
      RemoteDisplayInfo& display_info,
      mojom::LoginSessionInfoPtr session_reporter_info);

  // If `start_state_` is `STARTING` and there are no more session info queries
  // blocking startup, then transition to `STARTED` and run the init callback.
  void HandleSessionInfoQueriesBlockingStartup();

  void OnCreateDbusConnectionResult(
      base::expected<GDBusConnectionRef, Loggable> result);
  void OnGdmRemoteDisplayManagerStarted(base::expected<void, Loggable> result);

  // GdmRemoteDisplayManager::Observer:
  void OnRemoteDisplayCreated(
      const gvariant::ObjectPath& display_path,
      const GdmRemoteDisplayManager::RemoteDisplay& display) override;
  void OnRemoteDisplayRemoved(const gvariant::ObjectPath& display_path,
                              const gvariant::ObjectPath& remote_id) override;
  void OnRemoteDisplaySessionChanged(
      const gvariant::ObjectPath& display_path,
      const GdmRemoteDisplayManager::RemoteDisplay& display) override;

  // mojom::LoginSessionObserver:
  void OnLoginSessionCreated(mojom::LoginSessionInfoPtr session_info) override;

  void OnSessionInfoReady(
      const std::string& display_name,
      base::expected<LoginSessionManager::SessionInfo, Loggable> result);

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
  base::flat_map<std::string /*display_name*/, RemoteDisplayInfo>
      remote_displays_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Stores information from the session reporter process if received before the
  // information from the systemd D-BUS call.
  base::flat_map<std::string /*session_id*/, mojom::LoginSessionInfoPtr>
      pending_session_reporter_info_;

  // Tracks remote displays awaiting systemd session info which blocks the
  // startup process.
  // If there are remote displays with associated session IDs, that exist before
  // Start() was called, then the start callback won't be called until the
  // `session_info` of all of these remote displays have been populated. This
  // allows the caller to terminate all CRD-managed remote displays that were
  // leaked from the previous CRD host incarnation.
  base::flat_set<std::string /*display_name*/>
      session_info_queries_blocking_startup_
          GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<RemoteDisplaySessionManager> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_REMOTE_DISPLAY_SESSION_MANAGER_H_
