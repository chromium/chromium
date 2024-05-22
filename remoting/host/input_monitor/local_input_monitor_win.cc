// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_input_monitor_win.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/win/message_window.h"

namespace remoting {

namespace {

class LocalInputMonitorWinImpl : public LocalInputMonitorWin {
 public:
  LocalInputMonitorWinImpl(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      std::unique_ptr<RawInputHandler> raw_input_handler);

  LocalInputMonitorWinImpl(const LocalInputMonitorWinImpl&) = delete;
  LocalInputMonitorWinImpl& operator=(const LocalInputMonitorWinImpl&) = delete;

  ~LocalInputMonitorWinImpl() override;

 private:
  // The actual implementation resides in LocalInputMonitorWinImpl::Core class.
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
         std::unique_ptr<RawInputHandler> raw_input_handler);

    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    void Start();
    void Stop();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();

    void StartOnUiThread();
    void StopOnUiThread();

    // Handles WM_INPUT messages.
    LRESULT OnInput(HRAWINPUT input_handle);

    // Handles messages received by |window_|.
    bool HandleMessage(UINT message,
                       WPARAM wparam,
                       LPARAM lparam,
                       LRESULT* result);

    // Task runner on which public methods of this class must be called.
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

    // Task runner on which |window_| is created.
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

    // Used to receive raw input.
    std::unique_ptr<base::win::MessageWindow> window_;

    std::unique_ptr<RawInputHandler> raw_input_handler_;
  };

  scoped_refptr<Core> core_;

  SEQUENCE_CHECKER(sequence_checker_);
};

LocalInputMonitorWinImpl::LocalInputMonitorWinImpl(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    std::unique_ptr<RawInputHandler> raw_input_handler)
    : core_(new Core(caller_task_runner,
                     ui_task_runner,
                     std::move(raw_input_handler))) {
  core_->Start();
}

LocalInputMonitorWinImpl::~LocalInputMonitorWinImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  core_->Stop();
}

LocalInputMonitorWinImpl::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    std::unique_ptr<RawInputHandler> raw_input_handler)
    : caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      raw_input_handler_(std::move(raw_input_handler)) {}

void LocalInputMonitorWinImpl::Core::Start() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&Core::StartOnUiThread, this));
}

void LocalInputMonitorWinImpl::Core::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&Core::StopOnUiThread, this));
}

LocalInputMonitorWinImpl::Core::~Core() {
  DCHECK(!window_);
}

void LocalInputMonitorWinImpl::Core::StartOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  window_ = std::make_unique<base::win::MessageWindow>();
  if (!window_->Create(
          base::BindRepeating(&Core::HandleMessage, base::Unretained(this)))) {
    PLOG(ERROR) << "Failed to create the raw input window";
    window_.reset();

    // If the local input cannot be monitored, then bad things can happen like
    // the remote user taking over the session.
    raw_input_handler_->OnError();
  }
}

void LocalInputMonitorWinImpl::Core::StopOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Stop receiving raw mouse input.
  if (window_) {
    raw_input_handler_->Unregister();
  }

  window_.reset();
}

LRESULT LocalInputMonitorWinImpl::Core::OnInput(HRAWINPUT input_handle) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Get the size of the input record.
  UINT size = 0;
  UINT result = GetRawInputData(input_handle, RID_INPUT, nullptr, &size,
                                sizeof(RAWINPUTHEADER));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }

  // Retrieve the input record itself.
  auto buffer = base::HeapArray<char>::Uninit(size);
  RAWINPUT* input = reinterpret_cast<RAWINPUT*>(buffer.data());
  result = GetRawInputData(input_handle, RID_INPUT, buffer.data(), &size,
                           sizeof(RAWINPUTHEADER));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }

  raw_input_handler_->OnInputEvent(input);

  return DefRawInputProc(&input, 1, sizeof(RAWINPUTHEADER));
}

bool LocalInputMonitorWinImpl::Core::HandleMessage(UINT message,
                                                   WPARAM wparam,
                                                   LPARAM lparam,
                                                   LRESULT* result) {
  switch (message) {
    case WM_CREATE: {
      if (raw_input_handler_->Register(window_->hwnd())) {
        *result = 0;
      } else {
        *result = -1;
        raw_input_handler_->OnError();
      }
      return true;
    }

    case WM_INPUT:
      *result = OnInput(reinterpret_cast<HRAWINPUT>(lparam));
      return true;

    default:
      return false;
  }
}

}  // namespace

std::unique_ptr<LocalInputMonitorWin> LocalInputMonitorWin::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    std::unique_ptr<RawInputHandler> raw_input_handler) {
  return std::make_unique<LocalInputMonitorWinImpl>(
      caller_task_runner, ui_task_runner, std::move(raw_input_handler));
}

}  // namespace remoting
