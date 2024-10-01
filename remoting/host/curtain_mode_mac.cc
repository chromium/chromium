// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mode.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <Security/Security.h>
#include <unistd.h>

#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/login_util.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/base/errors.h"
#include "remoting/host/client_session_control.h"

namespace remoting {

namespace {

// Most machines will have < 4 displays but a larger upper bound won't hurt.
const UInt32 kMaxDisplaysToQuery = 32;

// 0x76697274 is a 4CC value for 'virt' which indicates the display is virtual.
const CGDirectDisplayID kVirtualDisplayID = 0x76697274;

// This method detects whether the local machine is running headless.
// Typically returns true when the session is curtained or if there are no
// physical monitors attached.  In those two scenarios, the online display will
// be marked as virtual.
bool IsRunningHeadless() {
  CGDirectDisplayID online_displays[kMaxDisplaysToQuery];
  UInt32 online_display_count = 0;
  CGError return_code = CGGetOnlineDisplayList(
      kMaxDisplaysToQuery, online_displays, &online_display_count);
  if (return_code != kCGErrorSuccess) {
    LOG(ERROR) << "CGGetOnlineDisplayList() failed: " << return_code;
    // If this fails, assume machine is headless to err on the side of caution.
    return true;
  }

  for (UInt32 i = 0; i < online_display_count; i++) {
    if (CGDisplayModelNumber(online_displays[i]) != kVirtualDisplayID) {
      // At least one monitor is attached so the machine is not headless.
      return false;
    }
  }

  return true;
}

// Used to detach the current session from the local console and disconnect
// the connection if it gets re-attached.
//
// Because the switch-in handler can only called on the main (UI) thread, this
// class installs the handler and detaches the current session from the console
// on the UI thread as well.
class SessionWatcher : public base::RefCountedThreadSafe<SessionWatcher> {
 public:
  SessionWatcher(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
                 scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
                 base::WeakPtr<ClientSessionControl> client_session_control);

  SessionWatcher(const SessionWatcher&) = delete;
  SessionWatcher& operator=(const SessionWatcher&) = delete;

  void Start();
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<SessionWatcher>;
  virtual ~SessionWatcher();

  // Detaches the session from the console and install the switch-in handler to
  // detect when the session re-attaches back.
  void ActivateCurtain();

  // Installs the switch-in handler.
  bool InstallEventHandler();

  // Removes the switch-in handler.
  void RemoveEventHandler();

  // Disconnects the client session.
  void DisconnectSession(ErrorCode error);

  // Handlers for the switch-in event.
  static OSStatus SessionActivateHandler(EventHandlerCallRef handler,
                                         EventRef event,
                                         void* user_data);

  // Task runner on which public methods of this class must be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner representing the thread receiving Carbon events.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Used to disconnect the client session.
  base::WeakPtr<ClientSessionControl> client_session_control_;

  EventHandlerRef event_handler_ = nullptr;
};

SessionWatcher::SessionWatcher(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      client_session_control_(client_session_control) {}

void SessionWatcher::Start() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Activate curtain asynchronously since it has to be done on the UI thread.
  // Because the curtain activation is asynchronous, it is possible that
  // the connection will not be curtained for a brief moment. This seems to be
  // unavoidable as long as the curtain enforcement depends on processing of
  // the switch-in notifications.
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SessionWatcher::ActivateCurtain, this));
}

void SessionWatcher::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  client_session_control_.reset();
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SessionWatcher::RemoveEventHandler, this));
}

SessionWatcher::~SessionWatcher() {
  DCHECK(!event_handler_);
}

void SessionWatcher::ActivateCurtain() {
  if (getuid() == 0) {
    // When curtain mode is in effect on Mac, the host process runs in the
    // user's switched-out session, but launchd will also run an instance at
    // the console login screen.  Even if no user is currently logged-on, we
    // can't support remote-access to the login screen because the current host
    // process model disconnects the client during login, which would leave
    // the logged in session un-curtained on the console until they reconnect.
    //
    // In case of fast user switch, there will be two host processes running,
    // one as the logged on user and another one as root. AgentProcessBroker
    // will terminate the root host process in that case.
    LOG(ERROR)
        << "Connecting to the console login session is not yet supported.";
    DisconnectSession(ErrorCode::LOGIN_SCREEN_NOT_SUPPORTED);
    return;
  }
  // Try to install the switch-in handler. Do this before switching out the
  // current session so that the console session is not affected if it fails.
  if (!InstallEventHandler()) {
    LOG(ERROR) << "Failed to install the switch-in handler.";
    DisconnectSession(ErrorCode::HOST_CONFIGURATION_ERROR);
    return;
  }

  base::apple::ScopedCFTypeRef<CFDictionaryRef> session(
      CGSessionCopyCurrentDictionary());

  // CGSessionCopyCurrentDictionary has been observed to return nullptr in some
  // cases. Once the system is in this state, curtain mode will fail as the
  // CGSession command thinks the session is not attached to the console. The
  // only known remedy is logout or reboot. Since we're not sure what causes
  // this, or how common it is, a crash report is useful in this case (note
  // that the connection would have to be refused in any case, so this is no
  // loss of functionality).
  CHECK(session) << "Error activating curtain-mode: "
                 << "CGSessionCopyCurrentDictionary() returned NULL. "
                 << "Logging out and back in should resolve this error.";

  const void* on_console =
      CFDictionaryGetValue(session.get(), kCGSessionOnConsoleKey);
  const void* logged_in =
      CFDictionaryGetValue(session.get(), kCGSessionLoginDoneKey);
  if (logged_in == kCFBooleanTrue && on_console == kCFBooleanTrue) {
    // If IsRunningHeadless() returns true then we know that the attempt to
    // switch to the login window will fail silently. This is a publicly known
    // issue.  We still want to try to curtain as the problem could be fixed in
    // a future OS release and the user could try reconnecting in that case
    // (until we had a real fix deployed). Issue is tracked via: rdar://42733382
    bool is_headless = IsRunningHeadless();

    std::optional<OSStatus> err = base::mac::SwitchToLoginWindow();
    if (!err.has_value()) {
      // Disconnect the session since we are unable to enter curtain mode.
      LOG(ERROR) << "SACSwitchToLoginWindow unavailable - unable to enter "
                    "curtain mode.";
      DisconnectSession(ErrorCode::HOST_CONFIGURATION_ERROR);
      return;
    }
    if (err.value() != noErr) {
      OSSTATUS_LOG(ERROR, err.value()) << "Failed to switch to login window";
      DisconnectSession(ErrorCode::HOST_CONFIGURATION_ERROR);
      return;
    }
    if (is_headless) {
      // Disconnect the session to prevent the user from unlocking the machine
      // since the call to SACSwitchToLoginWindow very likely failed. If we
      // allow them to unlock the machine, the local desktop would be visible if
      // the local monitor were plugged in.
      LOG(ERROR) << "Machine is running in headless mode (no monitors "
                 << "attached), we attempted to curtain the session but "
                 << "SACSwitchToLoginWindow is likely to fail in this mode.";
      DisconnectSession(ErrorCode::HOST_CONFIGURATION_ERROR);
      return;
    }
  }
}

bool SessionWatcher::InstallEventHandler() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(!event_handler_);

  EventTypeSpec event;
  event.eventClass = kEventClassSystem;
  event.eventKind = kEventSystemUserSessionActivated;
  OSStatus result = ::InstallApplicationEventHandler(
      NewEventHandlerUPP(SessionActivateHandler), 1, &event, this,
      &event_handler_);
  if (result != noErr) {
    event_handler_ = nullptr;
    DisconnectSession(ErrorCode::HOST_CONFIGURATION_ERROR);
    return false;
  }

  return true;
}

void SessionWatcher::RemoveEventHandler() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (event_handler_) {
    ::RemoveEventHandler(event_handler_);
    event_handler_ = nullptr;
  }
}

void SessionWatcher::DisconnectSession(ErrorCode error) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SessionWatcher::DisconnectSession, this, error));
    return;
  }

  if (client_session_control_) {
    client_session_control_->DisconnectSession(error);
  }
}

OSStatus SessionWatcher::SessionActivateHandler(EventHandlerCallRef handler,
                                                EventRef event,
                                                void* user_data) {
  static_cast<SessionWatcher*>(user_data)->DisconnectSession(ErrorCode::OK);
  return noErr;
}

}  // namespace

class CurtainModeMac : public CurtainMode {
 public:
  CurtainModeMac(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
                 scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
                 base::WeakPtr<ClientSessionControl> client_session_control);

  CurtainModeMac(const CurtainModeMac&) = delete;
  CurtainModeMac& operator=(const CurtainModeMac&) = delete;

  ~CurtainModeMac() override;

  // Overriden from CurtainMode.
  bool Activate() override;

 private:
  scoped_refptr<SessionWatcher> session_watcher_;
};

CurtainModeMac::CurtainModeMac(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : session_watcher_(new SessionWatcher(caller_task_runner,
                                          ui_task_runner,
                                          client_session_control)) {}

CurtainModeMac::~CurtainModeMac() {
  session_watcher_->Stop();
}

bool CurtainModeMac::Activate() {
  session_watcher_->Start();
  return true;
}

// static
std::unique_ptr<CurtainMode> CurtainMode::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return base::WrapUnique(new CurtainModeMac(caller_task_runner, ui_task_runner,
                                             client_session_control));
}

}  // namespace remoting
