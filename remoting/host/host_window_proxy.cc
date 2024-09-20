// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_window_proxy.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/client_session_control.h"
#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"

namespace remoting {

// Runs an instance of |HostWindow| on the |ui_task_runner_| thread.
class HostWindowProxy::Core : public base::RefCountedThreadSafe<Core>,
                              public ClientSessionControl {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
       scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
       std::unique_ptr<HostWindow> host_window);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  // Starts |host_window_| on the |ui_task_runner_| thread.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control);

  // Destroys |host_window_| on the |ui_task_runner_| thread.
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<Core>;
  ~Core() override;

  // Start() and Stop() equivalents called on the |ui_task_runner_| thread.
  void StartOnUiThread(const std::string& client_jid);
  void StopOnUiThread();

  // ClientSessionControl interface.
  const std::string& client_jid() const override;
  void DisconnectSession(protocol::ErrorCode error) override;
  void OnLocalKeyPressed(uint32_t usb_keycode) override;
  void OnLocalPointerMoved(const webrtc::DesktopVector& position,
                           ui::EventType type) override;
  void SetDisableInputs(bool disable_inputs) override;
  void OnDesktopDisplayChanged(
      std::unique_ptr<protocol::VideoLayout> layout) override;

  // Task runner on which public methods of this class must be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner on which |host_window_| is running.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Stores the client's JID so it can be read on the |ui_task_runner_| thread.
  std::string client_jid_;

  // Used to notify the caller about the local user's actions on
  // the |caller_task_runner| thread.
  base::WeakPtr<ClientSessionControl> client_session_control_;

  // The wrapped |HostWindow| instance running on the |ui_task_runner_| thread.
  std::unique_ptr<HostWindow> host_window_;

  // Used to create the control pointer passed to |host_window_|.
  base::WeakPtrFactory<ClientSessionControl> weak_factory_{this};
};

HostWindowProxy::HostWindowProxy(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    std::unique_ptr<HostWindow> host_window) {
  DCHECK(caller_task_runner->BelongsToCurrentThread());

  // Detach |host_window| from the calling thread so that |Core| could run it on
  // the |ui_task_runner_| thread.
  DETACH_FROM_SEQUENCE(host_window->sequence_checker_);
  core_ = new Core(caller_task_runner, ui_task_runner, std::move(host_window));
}

HostWindowProxy::~HostWindowProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  core_->Stop();
}

void HostWindowProxy::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  core_->Start(client_session_control);
}

HostWindowProxy::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    std::unique_ptr<HostWindow> host_window)
    : caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      host_window_(std::move(host_window)) {
  DCHECK(caller_task_runner->BelongsToCurrentThread());
}

void HostWindowProxy::Core::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!client_session_control_.get());
  DCHECK(client_session_control.get());

  client_session_control_ = client_session_control;
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::StartOnUiThread, this,
                                client_session_control->client_jid()));
}

void HostWindowProxy::Core::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&Core::StopOnUiThread, this));
}

HostWindowProxy::Core::~Core() {
  DCHECK(!host_window_);
}

void HostWindowProxy::Core::StartOnUiThread(const std::string& client_jid) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(client_jid_.empty());

  client_jid_ = client_jid;
  host_window_->Start(weak_factory_.GetWeakPtr());
}

void HostWindowProxy::Core::StopOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  host_window_.reset();
}

const std::string& HostWindowProxy::Core::client_jid() const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  return client_jid_;
}

void HostWindowProxy::Core::DisconnectSession(protocol::ErrorCode error) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::DisconnectSession, this, error));
    return;
  }

  if (client_session_control_.get()) {
    client_session_control_->DisconnectSession(error);
  }
}

void HostWindowProxy::Core::OnLocalKeyPressed(uint32_t usb_keycode) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::OnLocalKeyPressed, this, usb_keycode));
    return;
  }

  if (client_session_control_.get()) {
    client_session_control_->OnLocalKeyPressed(usb_keycode);
  }
}

void HostWindowProxy::Core::OnLocalPointerMoved(
    const webrtc::DesktopVector& position,
    ui::EventType type) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Core::OnLocalPointerMoved, this, position, type));
    return;
  }

  if (client_session_control_.get()) {
    client_session_control_->OnLocalPointerMoved(position, type);
  }
}

void HostWindowProxy::Core::SetDisableInputs(bool disable_inputs) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Core::SetDisableInputs, this, disable_inputs));
    return;
  }

  if (client_session_control_.get()) {
    client_session_control_->SetDisableInputs(disable_inputs);
  }
}

void HostWindowProxy::Core::OnDesktopDisplayChanged(
    std::unique_ptr<protocol::VideoLayout> layout) {
  NOTREACHED();
}

}  // namespace remoting
