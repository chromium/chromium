// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_hotkey_input_monitor.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform/platform_event_source.h"

namespace remoting {

namespace {

class LocalHotkeyInputMonitorChromeos : public LocalHotkeyInputMonitor {
 public:
  LocalHotkeyInputMonitorChromeos(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      base::OnceClosure disconnect_callback);

  LocalHotkeyInputMonitorChromeos(const LocalHotkeyInputMonitorChromeos&) =
      delete;
  LocalHotkeyInputMonitorChromeos& operator=(
      const LocalHotkeyInputMonitorChromeos&) = delete;

  ~LocalHotkeyInputMonitorChromeos() override;

 private:
  class Core : ui::PlatformEventObserver {
   public:
    Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         base::OnceClosure disconnect_callback);

    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    ~Core() override;

    void Start();

    // ui::PlatformEventObserver interface.
    void WillProcessEvent(const ui::PlatformEvent& event) override;
    void DidProcessEvent(const ui::PlatformEvent& event) override;

   private:
    void HandleKeyPressed(const ui::PlatformEvent& event);

    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

    // Must be called on |caller_task_runner_|.
    base::OnceClosure disconnect_callback_;
  };

  // Task runner on which ui::events are received.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  std::unique_ptr<Core> core_;
};

LocalHotkeyInputMonitorChromeos::LocalHotkeyInputMonitorChromeos(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    base::OnceClosure disconnect_callback)
    : input_task_runner_(input_task_runner),
      core_(new Core(caller_task_runner, std::move(disconnect_callback))) {
  input_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::Start, base::Unretained(core_.get())));
}

LocalHotkeyInputMonitorChromeos::~LocalHotkeyInputMonitorChromeos() {
  input_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

LocalHotkeyInputMonitorChromeos::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    base::OnceClosure disconnect_callback)
    : caller_task_runner_(caller_task_runner),
      disconnect_callback_(std::move(disconnect_callback)) {
  DCHECK(disconnect_callback_);
}

void LocalHotkeyInputMonitorChromeos::Core::Start() {
  // TODO(erg): Need to handle the mus case where PlatformEventSource is null
  // because we are in mus. This class looks like it can be rewritten with mus
  // EventMatchers. (And if that doesn't work, maybe a PointerObserver.)
  if (ui::PlatformEventSource::GetInstance()) {
    ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
  }
}

LocalHotkeyInputMonitorChromeos::Core::~Core() {
  if (ui::PlatformEventSource::GetInstance()) {
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
  }
}

void LocalHotkeyInputMonitorChromeos::Core::WillProcessEvent(
    const ui::PlatformEvent& event) {
  // No need to handle this callback.
}

void LocalHotkeyInputMonitorChromeos::Core::DidProcessEvent(
    const ui::PlatformEvent& event) {
  ui::EventType type = ui::EventTypeFromNative(event);
  if (type == ui::EventType::kKeyPressed) {
    HandleKeyPressed(event);
  }
}

void LocalHotkeyInputMonitorChromeos::Core::HandleKeyPressed(
    const ui::PlatformEvent& event) {
  // Ignore input if we've already initiated a disconnect.
  if (!disconnect_callback_) {
    return;
  }

  DCHECK(event->IsKeyEvent());
  ui::KeyEvent key_event(event);
  if (key_event.IsControlDown() && key_event.IsAltDown() &&
      key_event.key_code() == ui::VKEY_ESCAPE) {
    caller_task_runner_->PostTask(FROM_HERE, std::move(disconnect_callback_));
  }
}

}  // namespace

std::unique_ptr<LocalHotkeyInputMonitor> LocalHotkeyInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::OnceClosure disconnect_callback) {
  return std::make_unique<LocalHotkeyInputMonitorChromeos>(
      caller_task_runner, input_task_runner, std::move(disconnect_callback));
}

}  // namespace remoting
