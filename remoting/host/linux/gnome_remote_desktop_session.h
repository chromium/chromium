// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_REMOTE_DESKTOP_SESSION_H_
#define REMOTING_HOST_LINUX_GNOME_REMOTE_DESKTOP_SESSION_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "remoting/host/linux/ei_sender_session.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gnome_capture_stream_manager.h"
#include "remoting/host/linux/gnome_desktop_display_info_monitor.h"
#include "remoting/host/linux/gnome_desktop_resizer.h"
#include "remoting/host/linux/gnome_display_config_dbus_client.h"
#include "remoting/host/linux/gnome_display_config_monitor.h"
#include "remoting/host/linux/gnome_headless_detector.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/pipewire_mouse_cursor_capturer.h"
#include "remoting/host/persistent_display_layout_manager.h"

namespace remoting {

// A singleton for creating and managing a Gnome/Mutter remote desktop session.
// This allows the monitor layout and window configurations to persist after the
// CRD session has ended.
// Call GetInstance() to get the singleton instance, and call Init() to
// create a remote desktop session.
class GnomeRemoteDesktopSession {
 public:
  using InitCallbackSignature = void(base::expected<void, std::string>);
  using InitCallback = base::OnceCallback<InitCallbackSignature>;

  static bool IsRunningUnderGnome();

  // Returns the singleton instance of GnomeRemoteDesktopSession.
  static GnomeRemoteDesktopSession* GetInstance();

  GnomeRemoteDesktopSession(const GnomeRemoteDesktopSession&) = delete;
  GnomeRemoteDesktopSession& operator=(const GnomeRemoteDesktopSession&) =
      delete;

  // Creates a Gnome/Mutter remote desktop session, which includes creation of
  // a virtual monitor and the corresponding screencast stream, then calls
  // `callback`.
  // This method can be called multiple times. If an initialization is already
  // in progress, `callback` will be called after the initialization has
  // completed. If the remote desktop session is already initialized, nothing
  // happens, and `callback` will be called immediately by posting a task to the
  // current sequence. If initialization has failed, calling this again will
  // attempt to re-initialize.
  void Init(InitCallback callback);

  bool is_initialized() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return initialization_state_ == InitializationState::kInitialized;
  }

  base::WeakPtr<GnomeCaptureStreamManager> capture_stream_manager() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_initialized());
    return capture_stream_manager_.GetWeakPtr();
  }

  base::WeakPtr<PipewireMouseCursorCapturer> mouse_cursor_capturer() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_initialized());
    return mouse_cursor_capturer_.GetWeakPtr();
  }

  base::WeakPtr<GnomeDisplayConfigMonitor> display_config_monitor() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_initialized());
    return display_config_monitor_.GetWeakPtr();
  }

  base::WeakPtr<GnomeDesktopResizer> desktop_resizer() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_initialized());
    return desktop_resizer_.GetWeakPtr();
  }

  base::WeakPtr<EiSenderSession> ei_session() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_initialized());
    return ei_session_->GetWeakPtr();
  }

  GDBusConnectionRef connection() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_initialized());
    return connection_;
  }

  gvariant::ObjectPath session_path() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_initialized());
    return session_path_;
  }

  bool is_headless() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_initialized());
    return is_headless_;
  }

 private:
  friend class base::NoDestructor<GnomeRemoteDesktopSession>;

  enum class InitializationState {
    kNotInitialized,
    kInitializing,
    kInitialized,
  };

  GnomeRemoteDesktopSession();
  ~GnomeRemoteDesktopSession();

  template <typename SuccessType, typename String>
  GDBusConnectionRef::CallCallback<SuccessType> CheckResultAndContinue(
      void (GnomeRemoteDesktopSession::*success_method)(SuccessType),
      String&& error_context);
  void OnInitError(std::string_view what, Loggable why);
  void OnInitError(std::string_view what_and_why);
  void OnConnectionCreated(GDBusConnectionRef connection);
  void OnHeadlessDetection(bool is_headless);
  void OnSessionCreated(std::tuple<gvariant::ObjectPath> args);
  void OnGotSessionId(std::string session_id);
  void OnScreenCastSessionCreated(std::tuple<gvariant::ObjectPath> args);
  void OnSessionStarted(std::tuple<>);
  void OnEisFd(std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList> args);
  void OnEiSession(std::unique_ptr<EiSenderSession> ei_session);
  void OnDisplayConfigReceived(const GnomeDisplayConfig& config);

  SEQUENCE_CHECKER(sequence_checker_);

  InitializationState initialization_state_ GUARDED_BY_CONTEXT(
      sequence_checker_) = InitializationState::kNotInitialized;
  base::OnceCallbackList<InitCallbackSignature> init_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);
  GDBusConnectionRef connection_ GUARDED_BY_CONTEXT(sequence_checker_);
  gvariant::ObjectPath session_path_ GUARDED_BY_CONTEXT(sequence_checker_);
  gvariant::ObjectPath screencast_session_path_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<EiSenderSession> ei_session_
      GUARDED_BY_CONTEXT(sequence_checker_);
  GnomeDisplayConfigDBusClient display_config_client_
      GUARDED_BY_CONTEXT(sequence_checker_);
  GnomeDisplayConfigMonitor display_config_monitor_ GUARDED_BY_CONTEXT(
      sequence_checker_){display_config_client_.GetWeakPtr()};
  GnomeCaptureStreamManager capture_stream_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);
  PipewireMouseCursorCapturer mouse_cursor_capturer_ GUARDED_BY_CONTEXT(
      sequence_checker_){std::make_unique<GnomeDesktopDisplayInfoMonitor>(
                             display_config_monitor_.GetWeakPtr()),
                         capture_stream_manager_.GetWeakPtr()};
  GnomeDesktopResizer desktop_resizer_ GUARDED_BY_CONTEXT(sequence_checker_){
      capture_stream_manager_.GetWeakPtr(), display_config_client_.GetWeakPtr(),
      display_config_monitor_.GetWeakPtr()};
  PersistentDisplayLayoutManager persistent_display_layout_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<GnomeDisplayConfigMonitor::Subscription>
      display_config_subscription_ GUARDED_BY_CONTEXT(sequence_checker_);
  GnomeHeadlessDetector headless_detector_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool is_headless_ GUARDED_BY_CONTEXT(sequence_checker_){false};

  base::WeakPtrFactory<GnomeRemoteDesktopSession> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_REMOTE_DESKTOP_SESSION_H_
