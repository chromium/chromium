// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/desktop_event_handler.h"

#include <windows.h>

#include <WinUser.h>

#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_local.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/logging.h"
#include "third_party/webrtc/modules/desktop_capture/win/desktop.h"

namespace remoting {

namespace {

constexpr base::TimeDelta kCheckInputDesktopInterval = base::Seconds(1);

std::unique_ptr<webrtc::Desktop> SwitchToInputDesktop() {
  std::unique_ptr<webrtc::Desktop> input_desktop(
      webrtc::Desktop::GetInputDesktop());
  if (!input_desktop) {
    PLOG(ERROR) << "GetInputDesktop failed";
    return nullptr;
  }
  bool succeed = input_desktop->SetThreadDesktop();
  if (!succeed) {
    PLOG(ERROR) << "SetThreadDesktop failed";
    return nullptr;
  }
  return input_desktop;
}

}  // namespace

class DesktopEventHandler::Core : public base::RefCountedThreadSafe<Core> {
 public:
  Core(DWORD min_event, DWORD max_event, std::unique_ptr<Delegate> delegate);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  void Start();
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<Core>;
  ~Core();

  // Methods below are called on the worker thread.

  // We are using TLS with a scoped_refptr here to hold a reference to `Core`
  // until the worker thread is deleted. SetWinEventHook() runs callbacks on the
  // calling thread, so TLS is used to allow for multiple instances of
  // DesktopEventHandler. It also allows the reference to be released when the
  // thread is stopped. During destruction of `DesktopEventHandler`,
  // `DesktopEventHandler` releases its reference to `Core`, and calls
  // Core::Stop(), which destroys the worker thread and releases the thread's
  // reference to `Core` and triggers `Core`'s destructor.
  static base::ThreadLocalOwnedPointer<scoped_refptr<Core>>&
  GetCoreTlsPointer();
  static void CALLBACK HandleWinEvent(HWINEVENTHOOK hWinEventHook,
                                      DWORD event,
                                      HWND hwnd,
                                      LONG idObject,
                                      LONG idChild,
                                      DWORD dwEventThread,
                                      DWORD dwmsEventTime);
  void OnBeforeInitWorkerThread();
  void OnWorkerThreadStarted();
  void CheckInputDesktop();
  void DestroyWorkerThread();

  SEQUENCE_CHECKER(caller_sequence_checker_);
  SEQUENCE_CHECKER(worker_sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> caller_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  const DWORD min_event_;
  const DWORD max_event_;

  base::Lock delegate_lock_;
  // This is set to null after Stop() is called, i.e., DesktopEventHandler has
  // been destroyed.
  std::unique_ptr<Delegate> delegate_ GUARDED_BY(delegate_lock_);

  // Fields below are only initialized after the worker thread has started.
  std::unique_ptr<webrtc::Desktop> desktop_;
  HWINEVENTHOOK win_event_hook_ GUARDED_BY_CONTEXT(worker_sequence_checker_) =
      nullptr;
  base::RepeatingTimer check_input_desktop_timer_
      GUARDED_BY_CONTEXT(worker_sequence_checker_);
};

DesktopEventHandler::Core::Core(DWORD min_event,
                                DWORD max_event,
                                std::unique_ptr<Delegate> delegate)
    : min_event_(min_event),
      max_event_(max_event),
      delegate_(std::move(delegate)) {
  caller_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
}

DesktopEventHandler::Core::~Core() {
  base::AutoLock lock(delegate_lock_);
  DCHECK(!delegate_) << "Stop() must be called before Core is destructed.";
  DCHECK(!worker_task_runner_);
}

void DesktopEventHandler::Core::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(caller_sequence_checker_);
  DCHECK(caller_task_runner_);
  DCHECK(!worker_task_runner_);
  DETACH_FROM_SEQUENCE(worker_sequence_checker_);

  {
    base::AutoLock lock(delegate_lock_);
    if (!delegate_) {
      // DesktopEventHandler is already deleted, so don't start the event
      // thread.
      return;
    }
  }

  // A UI thread is required for SetWinEventHook(). However, this means
  // SetThreadDesktop() won't work after the message pump has started, so we
  // need to call SetWinEventHook() in the pre-init callback. See comments in
  // OnBeforeInitWorkerThread().
  // OnBeforeInitWorkerThread() and OnWorkerThreadStarted() may be called after
  // Stop() is called, but this is fine since Stop() posts a task to call
  // DestroyWorkerThread() after them.
  worker_task_runner_ = remoting::AutoThread::CreateWithPreInitCallback(
      "WinEventWorkerThread", /* joiner= */ caller_task_runner_,
      base::MessagePumpType::UI,
      base::BindOnce(&Core::OnBeforeInitWorkerThread, this));
  worker_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::OnWorkerThreadStarted, this));
}

void DesktopEventHandler::Core::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(caller_sequence_checker_);
  base::AutoLock lock(delegate_lock_);
  delegate_.reset();
  if (worker_task_runner_) {
    worker_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::DestroyWorkerThread, this));
  }
}

// static
base::ThreadLocalOwnedPointer<scoped_refptr<DesktopEventHandler::Core>>&
DesktopEventHandler::Core::GetCoreTlsPointer() {
  static base::NoDestructor<base::ThreadLocalOwnedPointer<scoped_refptr<Core>>>
      core_tls;
  return *core_tls;
}

// static
void CALLBACK
DesktopEventHandler::Core::HandleWinEvent(HWINEVENTHOOK hWinEventHook,
                                          DWORD event,
                                          HWND hwnd,
                                          LONG idObject,
                                          LONG idChild,
                                          DWORD dwEventThread,
                                          DWORD dwmsEventTime) {
  Core& core = **GetCoreTlsPointer();
  base::AutoLock lock(core.delegate_lock_);
  if (core.delegate_) {
    core.delegate_->OnEvent(event, idObject);
  }
}

void DesktopEventHandler::Core::OnBeforeInitWorkerThread() {
  // This method is run on the worker thread to switch to the current input
  // desktop before the message pump has started. Once the message pump has
  // started, a message window will be created on `desktop_` and calls to
  // SetThreadDesktop() will no longer be allowed.
  desktop_ = SwitchToInputDesktop();
  if (!desktop_) {
    // We can't just CHECK it because SwitchToInputDesktop() fails in unit
    // tests.
    LOG(ERROR) << "Failed to switch to input desktop.";
  }
  auto& core_tls = GetCoreTlsPointer();
  core_tls.Set(std::make_unique<scoped_refptr<Core>>(this));
}

void DesktopEventHandler::Core::OnWorkerThreadStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);

  win_event_hook_ =
      SetWinEventHook(min_event_, max_event_, nullptr, &HandleWinEvent,
                      /*idProcess=*/0, /*idThread=*/0, WINEVENT_OUTOFCONTEXT);
  CHECK(win_event_hook_);

  check_input_desktop_timer_.Start(FROM_HERE, kCheckInputDesktopInterval, this,
                                   &Core::CheckInputDesktop);
  {
    base::AutoLock lock(delegate_lock_);
    if (delegate_) {
      delegate_->OnWorkerThreadStarted();
    }
  }
}

void DesktopEventHandler::Core::CheckInputDesktop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
  DCHECK(desktop_);

  // Check if the desktop associated with the worker thread is still the current
  // input desktop. If it's not the case, we restart the worker thread, since it
  // is not possible to change the desktop on the worker thread directly.
  auto current_desktop = base::WrapUnique(webrtc::Desktop::GetInputDesktop());
  if (desktop_->IsSame(*current_desktop)) {
    return;
  }
  HOST_LOG << "The input desktop has changed. The worker thread will be "
           << "restarted with the new input desktop.";
  DestroyWorkerThread();
  caller_task_runner_->PostTask(FROM_HERE, base::BindOnce(&Core::Start, this));
}

void DesktopEventHandler::Core::DestroyWorkerThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);

  check_input_desktop_timer_.Stop();
  if (win_event_hook_) {
    UnhookWinEvent(win_event_hook_);
    win_event_hook_ = nullptr;
  }
  worker_task_runner_ = nullptr;
}

// DesktopEventHandler implementation

DesktopEventHandler::DesktopEventHandler() = default;

DesktopEventHandler::~DesktopEventHandler() {
  if (core_) {
    core_->Stop();
  }
}

void DesktopEventHandler::Start(DWORD min_event,
                                DWORD max_event,
                                std::unique_ptr<Delegate> delegate) {
  CHECK_LE(min_event, max_event);
  core_ = base::MakeRefCounted<Core>(min_event, max_event, std::move(delegate));
  core_->Start();
}

}  // namespace remoting
