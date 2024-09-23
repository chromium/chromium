// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_pointer_input_monitor.h"

#include <memory>
#include <utility>

#include "ash/shell.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/chromeos/point_transformer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_target.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform/platform_event_source.h"

namespace remoting {

namespace {

bool IsInjectedByCrd(const ui::PlatformEvent& event) {
  return event->source_device_id() == ui::ED_REMOTE_INPUT_DEVICE;
}

class LocalPointerInputMonitorChromeos : public LocalPointerInputMonitor {
 public:
  LocalPointerInputMonitorChromeos(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      LocalInputMonitor::PointerMoveCallback on_pointer_move);

  LocalPointerInputMonitorChromeos(const LocalPointerInputMonitorChromeos&) =
      delete;
  LocalPointerInputMonitorChromeos& operator=(
      const LocalPointerInputMonitorChromeos&) = delete;

  ~LocalPointerInputMonitorChromeos() override;

 private:
  class Core : ui::PlatformEventObserver {
   public:
    Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         LocalInputMonitor::PointerMoveCallback on_pointer_move);

    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    ~Core() override;

    void Start();

    // ui::PlatformEventObserver interface.
    void WillProcessEvent(const ui::PlatformEvent& event) override;
    void DidProcessEvent(const ui::PlatformEvent& event) override;

   private:
    void HandlePointerMove(const ui::PlatformEvent& event, ui::EventType type);

    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

    // Used to send pointer event notifications.
    // Must be called on the |caller_task_runner_|.
    LocalInputMonitor::PointerMoveCallback on_pointer_move_;
  };

  // Task runner on which ui::events are received.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  std::unique_ptr<Core> core_;
};

LocalPointerInputMonitorChromeos::LocalPointerInputMonitorChromeos(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    LocalInputMonitor::PointerMoveCallback on_pointer_move)
    : input_task_runner_(input_task_runner),
      core_(new Core(caller_task_runner, std::move(on_pointer_move))) {
  input_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::Start, base::Unretained(core_.get())));
}

LocalPointerInputMonitorChromeos::~LocalPointerInputMonitorChromeos() {
  input_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

LocalPointerInputMonitorChromeos::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    LocalInputMonitor::PointerMoveCallback on_pointer_move)
    : caller_task_runner_(caller_task_runner),
      on_pointer_move_(std::move(on_pointer_move)) {}

void LocalPointerInputMonitorChromeos::Core::Start() {
  // TODO(erg): Need to handle the mus case where PlatformEventSource is null
  // because we are in mus. This class looks like it can be rewritten with mus
  // EventMatchers. (And if that doesn't work, maybe a PointerObserver.)
  if (ui::PlatformEventSource::GetInstance()) {
    ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
  }
}

LocalPointerInputMonitorChromeos::Core::~Core() {
  if (ui::PlatformEventSource::GetInstance()) {
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
  }
}

void LocalPointerInputMonitorChromeos::Core::WillProcessEvent(
    const ui::PlatformEvent& event) {
  // No need to handle this callback.
}

void LocalPointerInputMonitorChromeos::Core::DidProcessEvent(
    const ui::PlatformEvent& event) {
  // Do not pass on events remotely injected by CRD, as we're supposed to
  // monitor for local input only.
  if (IsInjectedByCrd(event)) {
    return;
  }

  ui::EventType type = ui::EventTypeFromNative(event);
  if (type == ui::EventType::kMouseMoved ||
      type == ui::EventType::kTouchMoved) {
    HandlePointerMove(event, type);
  }
}

void LocalPointerInputMonitorChromeos::Core::HandlePointerMove(
    const ui::PlatformEvent& event,
    ui::EventType type) {
  ui::LocatedEvent* located_event = event->AsLocatedEvent();
  // The event we received has the location of the mouse in pixels
  // *within the current display*. The event itself does not tell us what
  // display the mouse is on (so the top-left of every display has coordinates
  // 0x0 in the event).
  // Luckily the cursor manager remembers the display the mouse is on.
  const display::Display& current_display =
      ash::Shell::Get()->cursor_manager()->GetDisplay();
  const aura::Window& window = CHECK_DEREF(
      ash::Shell::Get()->GetRootWindowForDisplayId(current_display.id()));

  gfx::PointF location_in_window_in_pixels = located_event->location_f();

  gfx::PointF location_in_screen_in_dip =
      PointTransformer::ConvertWindowInPixelToScreenInDip(
          window, location_in_window_in_pixels);

  gfx::Point pointer_position = gfx::ToRoundedPoint(location_in_screen_in_dip);

  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(on_pointer_move_,
                                webrtc::DesktopVector(pointer_position.x(),
                                                      pointer_position.y()),
                                type));
}

}  // namespace

std::unique_ptr<LocalPointerInputMonitor> LocalPointerInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::PointerMoveCallback on_pointer_move,
    base::OnceClosure disconnect_callback) {
  return std::make_unique<LocalPointerInputMonitorChromeos>(
      caller_task_runner, input_task_runner, std::move(on_pointer_move));
}

}  // namespace remoting
