// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_pointer_input_monitor.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/chromeos/point_transformer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform/platform_event_source.h"

namespace remoting {

namespace {

class LocalPointerInputMonitorChromeos : public LocalPointerInputMonitor {
 public:
  LocalPointerInputMonitorChromeos(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      LocalInputMonitor::PointerMoveCallback on_pointer_move);
  ~LocalPointerInputMonitorChromeos() override;

 private:
  class Core : ui::PlatformEventObserver {
   public:
    Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         LocalInputMonitor::PointerMoveCallback on_pointer_move);
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

    // Used to rotate the local pointer positions appropriately based on the
    // current display rotation settings.
    std::unique_ptr<PointTransformer> point_transformer_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  // Task runner on which ui::events are received.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  std::unique_ptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(LocalPointerInputMonitorChromeos);
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
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
  point_transformer_.reset(new PointTransformer());
}

LocalPointerInputMonitorChromeos::Core::~Core() {
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
}

void LocalPointerInputMonitorChromeos::Core::WillProcessEvent(
    const ui::PlatformEvent& event) {
  // No need to handle this callback.
}

void LocalPointerInputMonitorChromeos::Core::DidProcessEvent(
    const ui::PlatformEvent& event) {
  ui::EventType type = ui::EventTypeFromNative(event);
  if (type == ui::ET_MOUSE_MOVED || type == ui::ET_TOUCH_MOVED) {
    HandlePointerMove(event, type);
  }
}

void LocalPointerInputMonitorChromeos::Core::HandlePointerMove(
    const ui::PlatformEvent& event,
    ui::EventType type) {
  auto position = gfx::PointF(ui::EventLocationFromNative(event));
  position = point_transformer_->FromScreenCoordinates(position);

  caller_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(on_pointer_move_,
                     webrtc::DesktopVector(position.x(), position.y()), type));
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
