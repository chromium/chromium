// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PORTAL_REMOTE_DESKTOP_SESSION_H_
#define REMOTING_HOST_LINUX_PORTAL_REMOTE_DESKTOP_SESSION_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/linux/ei_sender_session.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/pipewire_mouse_cursor_capturer.h"
#include "remoting/host/linux/portal_capture_stream_manager.h"
#include "remoting/host/linux/portal_desktop_resizer.h"
#include "remoting/host/linux/scoped_portal_request.h"
#include "remoting/host/linux/scoped_portal_session.h"
#include "remoting/host/polling_desktop_display_info_monitor.h"

namespace remoting {

// A singleton for creating and managing a Portal-based remote desktop session.
class PortalRemoteDesktopSession {
 public:
  using InitCallbackSignature = void(base::expected<void, std::string>);
  using InitCallback = base::OnceCallback<InitCallbackSignature>;

  // Returns the singleton instance.
  static PortalRemoteDesktopSession* GetInstance();

  PortalRemoteDesktopSession(const PortalRemoteDesktopSession&) = delete;
  PortalRemoteDesktopSession& operator=(const PortalRemoteDesktopSession&) =
      delete;

  // Initializes the PortalRemoteDesktopSession, which includes creation of a
  // virtual monitor and the corresponding screencast stream, then calls
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

  base::WeakPtr<PortalCaptureStreamManager> capture_stream_manager() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_initialized());
    return capture_stream_manager_.GetWeakPtr();
  }

  base::WeakPtr<PipewireMouseCursorCapturer> mouse_cursor_capturer() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_initialized());
    return mouse_cursor_capturer_->GetWeakPtr();
  }

  base::WeakPtr<PortalDesktopResizer> desktop_resizer() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_initialized());
    return desktop_resizer_.GetWeakPtr();
  }

  base::WeakPtr<EiSenderSession> ei_session() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_initialized());
    return ei_session_->GetWeakPtr();
  }

  base::WeakPtr<DesktopDisplayInfoMonitor> display_info_monitor() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_initialized());
    return display_info_monitor_->GetWeakPtr();
  }

 private:
  friend class base::NoDestructor<PortalRemoteDesktopSession>;

  enum class InitializationState {
    kNotInitialized,
    kInitializing,
    kInitialized,
  };

  PortalRemoteDesktopSession();
  ~PortalRemoteDesktopSession();

  // Calls `success_method` if the returned callback is called with a result,
  // otherwise calls OnInitError().
  // Resets `request` iff it is not a nullptr (i.e. pointing to a unique_ptr).
  template <typename SuccessType, typename String>
  GDBusConnectionRef::CallCallback<SuccessType> CheckResultAndContinue(
      void (PortalRemoteDesktopSession::*success_method)(SuccessType),
      std::unique_ptr<ScopedPortalRequest>* request,
      String&& error_context);

  // No-op if the returned callback is called with a result, otherwise resets
  // `request` and calls OnInitError().
  template <typename String>
  GDBusConnectionRef::CallCallback<GVariantRef<"(o)">> ResetRequestOnFailure(
      std::unique_ptr<ScopedPortalRequest>& request,
      String&& error_context);

  void OnInitError(std::string_view what, Loggable why);
  void OnConnectionCreated(GDBusConnectionRef connection);
  void OnCreateSessionResponse(gvariant::GVariantRef<"a{sv}"> result);
  void OnSelectDevicesResponse(gvariant::GVariantRef<"a{sv}"> result);
  void OnCaptureStreamInitResult(base::expected<void, std::string> result);
  void OnEisFd(std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList> args);
  void OnEiSession(std::unique_ptr<EiSenderSession> ei_session);
  void OnSessionClosed(gvariant::GVariantRef<"a{sv}"> details);

  SEQUENCE_CHECKER(sequence_checker_);

  InitializationState initialization_state_ GUARDED_BY_CONTEXT(
      sequence_checker_) = InitializationState::kNotInitialized;
  base::OnceCallbackList<InitCallbackSignature> init_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);
  GDBusConnectionRef connection_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<ScopedPortalSession> portal_session_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<ScopedPortalRequest> create_session_request_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<ScopedPortalRequest> select_devices_request_
      GUARDED_BY_CONTEXT(sequence_checker_);

  PortalCaptureStreamManager capture_stream_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);
  PortalDesktopResizer desktop_resizer_ GUARDED_BY_CONTEXT(sequence_checker_){
      capture_stream_manager_};
  std::unique_ptr<PollingDesktopDisplayInfoMonitor> display_info_monitor_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<PipewireMouseCursorCapturer> mouse_cursor_capturer_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<EiSenderSession> ei_session_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<PortalRemoteDesktopSession> weak_ptr_factory_{this};
};
}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PORTAL_REMOTE_DESKTOP_SESSION_H_
