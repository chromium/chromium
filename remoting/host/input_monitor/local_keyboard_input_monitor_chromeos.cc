// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_keyboard_input_monitor.h"

#include <cstdint>
#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_observer.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform/platform_event_source.h"

namespace remoting {

namespace {

bool IsInjectedByCrd(const ui::Event& event) {
  return event.source_device_id() == ui::ED_REMOTE_INPUT_DEVICE;
}

class LocalKeyboardInputMonitorChromeos : public LocalKeyboardInputMonitor {
 public:
  LocalKeyboardInputMonitorChromeos(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      LocalInputMonitor::KeyPressedCallback key_pressed_callback);

  LocalKeyboardInputMonitorChromeos(const LocalKeyboardInputMonitorChromeos&) =
      delete;
  LocalKeyboardInputMonitorChromeos& operator=(
      const LocalKeyboardInputMonitorChromeos&) = delete;

  ~LocalKeyboardInputMonitorChromeos() override;

 private:
  class Core : public ui::PlatformEventObserver,
               public ui::EventObserver,
               public aura::EnvObserver {
   public:
    Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         LocalInputMonitor::KeyPressedCallback on_key_event_callback);

    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    ~Core() override;

    void Start();

    // ui::PlatformEventObserver interface.
    void WillProcessEvent(const ui::PlatformEvent& event) override;
    void DidProcessEvent(const ui::PlatformEvent& event) override;

    // ui::EventObserver interface.
    void OnEvent(const ui::Event& event) override;

    // aura::EnvObserver interface.
    void OnWillDestroyEnv() override;

   private:
    void StopObservingEnv();

    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
    LocalInputMonitor::KeyPressedCallback key_pressed_callback_;
  };

  // Task runner on which ui::events are received.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  std::unique_ptr<Core> core_;
};

LocalKeyboardInputMonitorChromeos::LocalKeyboardInputMonitorChromeos(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    LocalInputMonitor::KeyPressedCallback key_pressed_callback)
    : input_task_runner_(input_task_runner),
      core_(new Core(caller_task_runner, std::move(key_pressed_callback))) {
  input_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::Start, base::Unretained(core_.get())));
}

LocalKeyboardInputMonitorChromeos::~LocalKeyboardInputMonitorChromeos() {
  input_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

LocalKeyboardInputMonitorChromeos::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    LocalInputMonitor::KeyPressedCallback key_pressed_callback)
    : caller_task_runner_(caller_task_runner),
      key_pressed_callback_(std::move(key_pressed_callback)) {}

void LocalKeyboardInputMonitorChromeos::Core::Start() {
  // TODO(erg): Need to handle the mus case where PlatformEventSource is null
  // because we are in mus. This class looks like it can be rewritten with mus
  // EventMatchers.
  if (ui::PlatformEventSource::GetInstance()) {
    ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
  }
  if (aura::Env::HasInstance()) {
    aura::Env::GetInstance()->AddObserver(this);
    aura::Env::GetInstance()->AddEventObserver(this, aura::Env::GetInstance(),
                                               {ui::EventType::kKeyPressed});
  }
}

LocalKeyboardInputMonitorChromeos::Core::~Core() {
  StopObservingEnv();
  if (ui::PlatformEventSource::GetInstance()) {
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
  }
}

void LocalKeyboardInputMonitorChromeos::Core::WillProcessEvent(
    const ui::PlatformEvent& event) {
  // No need to handle this callback.
}

void LocalKeyboardInputMonitorChromeos::Core::DidProcessEvent(
    const ui::PlatformEvent& event) {
  OnEvent(*event);
}

void LocalKeyboardInputMonitorChromeos::Core::OnEvent(const ui::Event& event) {
  // Do not pass on events remotely injected by CRD, as we're supposed to
  // monitor for local input only.
  if (IsInjectedByCrd(event)) {
    return;
  }

  if (event.type() == ui::EventType::kKeyPressed) {
    uint32_t usb_keycode =
        ui::KeycodeConverter::DomCodeToUsbKeycode(event.AsKeyEvent()->code());
    caller_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(key_pressed_callback_, usb_keycode));
  }
}

void LocalKeyboardInputMonitorChromeos::Core::OnWillDestroyEnv() {
  StopObservingEnv();
}

void LocalKeyboardInputMonitorChromeos::Core::StopObservingEnv() {
  if (aura::Env::HasInstance()) {
    aura::Env::GetInstance()->RemoveEventObserver(this);
    aura::Env::GetInstance()->RemoveObserver(this);
  }
}

}  // namespace

std::unique_ptr<LocalKeyboardInputMonitor> LocalKeyboardInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::KeyPressedCallback on_key_event_callback,
    base::OnceClosure disconnect_callback) {
  return std::make_unique<LocalKeyboardInputMonitorChromeos>(
      caller_task_runner, input_task_runner, std::move(on_key_event_callback));
}

}  // namespace remoting
