// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// Related header must be first according to Google C++ style guide.
#include "remoting/host/input_monitor/local_pointer_input_monitor.h"
// clang-format on

#import <AppKit/AppKit.h>
#include <unistd.h>

#include <utility>

#include "base/apple/scoped_cftyperef.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/types/event_type.h"

namespace remoting {
namespace {

// Note that this class does not detect touch input and so is named accordingly.
class LocalMouseInputMonitorMac : public LocalPointerInputMonitor {
 public:
  // Invoked by LocalInputMonitorManager.
  class EventHandler {
   public:
    virtual ~EventHandler() = default;

    virtual void OnLocalPointerEvent(const webrtc::DesktopVector& position,
                                     ui::EventType type) = 0;
  };

  LocalMouseInputMonitorMac(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      LocalInputMonitor::PointerMoveCallback on_mouse_move);

  LocalMouseInputMonitorMac(const LocalMouseInputMonitorMac&) = delete;
  LocalMouseInputMonitorMac& operator=(const LocalMouseInputMonitorMac&) =
      delete;

  ~LocalMouseInputMonitorMac() override;

 private:
  // The actual implementation resides in LocalMouseInputMonitorMac::Core class.
  class Core;
  scoped_refptr<Core> core_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace
}  // namespace remoting

@interface LocalInputMonitorManager : NSObject {
 @private
  base::apple::ScopedCFTypeRef<CFRunLoopSourceRef> _mouseRunLoopSource;
  base::apple::ScopedCFTypeRef<CFMachPortRef> _mouseMachPort;
  raw_ptr<remoting::LocalMouseInputMonitorMac::EventHandler> _monitor;
}

- (instancetype)initWithMonitor:
    (remoting::LocalMouseInputMonitorMac::EventHandler*)monitor;

// Called when local pointer input is detected.
- (void)localPointerEvent:(const webrtc::DesktopVector&)mousePos
                     type:(ui::EventType)type;

// Re-enables the event tap if disabled by timeout.
- (void)reEnableEventTap;

// Must be called when the LocalInputMonitorManager is no longer to be used.
// Similar to NSTimer in that more than a simple release is required.
- (void)invalidate;

@end

static CGEventRef LocalPointerEventCallback(CGEventTapProxy proxy,
                                            CGEventType type,
                                            CGEventRef event,
                                            void* context) {
  LocalInputMonitorManager* manager =
      (__bridge LocalInputMonitorManager*)context;
  if (type == kCGEventTapDisabledByTimeout) {
    [manager reEnableEventTap];
    return event;
  }
  if (type == kCGEventTapDisabledByUserInput) {
    return event;
  }

  // Filter out events injected by the CRD host process, while capturing
  // hardware events (pid == 0) and synthetic events from other local software
  // (e.g. accessibility and assistive tools).
  int64_t pid = CGEventGetIntegerValueField(event, kCGEventSourceUnixProcessID);
  if (pid != getpid()) {
    ui::EventType ui_event_type;
    switch (type) {
      case kCGEventLeftMouseDown:
      case kCGEventRightMouseDown:
      case kCGEventOtherMouseDown:
        ui_event_type = ui::EventType::kMousePressed;
        break;
      case kCGEventLeftMouseUp:
      case kCGEventRightMouseUp:
      case kCGEventOtherMouseUp:
        ui_event_type = ui::EventType::kMouseReleased;
        break;
      case kCGEventLeftMouseDragged:
      case kCGEventRightMouseDragged:
      case kCGEventOtherMouseDragged:
        ui_event_type = ui::EventType::kMouseDragged;
        break;
      case kCGEventScrollWheel:
        ui_event_type = ui::EventType::kMousewheel;
        break;
      case kCGEventMouseMoved:
      default:
        ui_event_type = ui::EventType::kMouseMoved;
        break;
    }

    CGPoint cgMousePos = CGEventGetLocation(event);
    webrtc::DesktopVector mousePos(cgMousePos.x, cgMousePos.y);
    [manager localPointerEvent:mousePos type:ui_event_type];
  }
  return event;
}

@implementation LocalInputMonitorManager

- (instancetype)initWithMonitor:
    (remoting::LocalMouseInputMonitorMac::EventHandler*)monitor {
  if ((self = [super init])) {
    _monitor = monitor;

    const CGEventMask mouse_mask = CGEventMaskBit(kCGEventMouseMoved) |
                                   CGEventMaskBit(kCGEventLeftMouseDown) |
                                   CGEventMaskBit(kCGEventLeftMouseUp) |
                                   CGEventMaskBit(kCGEventLeftMouseDragged) |
                                   CGEventMaskBit(kCGEventRightMouseDown) |
                                   CGEventMaskBit(kCGEventRightMouseUp) |
                                   CGEventMaskBit(kCGEventRightMouseDragged) |
                                   CGEventMaskBit(kCGEventOtherMouseDown) |
                                   CGEventMaskBit(kCGEventOtherMouseUp) |
                                   CGEventMaskBit(kCGEventOtherMouseDragged) |
                                   CGEventMaskBit(kCGEventScrollWheel);

    _mouseMachPort.reset(CGEventTapCreate(
        kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly,
        mouse_mask, LocalPointerEventCallback, (__bridge void*)self));
    if (_mouseMachPort) {
      _mouseRunLoopSource.reset(
          CFMachPortCreateRunLoopSource(nullptr, _mouseMachPort.get(), 0));
      CFRunLoopAddSource(CFRunLoopGetMain(), _mouseRunLoopSource.get(),
                         kCFRunLoopCommonModes);
    } else {
      LOG(ERROR) << "CGEventTapCreate failed.";
      self = nil;
      return nil;
    }
  }
  return self;
}

- (void)localPointerEvent:(const webrtc::DesktopVector&)mousePos
                     type:(ui::EventType)type {
  _monitor->OnLocalPointerEvent(mousePos, type);
}

- (void)reEnableEventTap {
  if (_mouseMachPort) {
    CGEventTapEnable(_mouseMachPort.get(), true);
  }
}

- (void)invalidate {
  if (_mouseRunLoopSource) {
    CFMachPortInvalidate(_mouseMachPort.get());
    CFRunLoopRemoveSource(CFRunLoopGetMain(), _mouseRunLoopSource.get(),
                          kCFRunLoopCommonModes);
    _mouseMachPort.reset();
    _mouseRunLoopSource.reset();
  }
}

@end

namespace remoting {
namespace {

class LocalMouseInputMonitorMac::Core : public base::RefCountedThreadSafe<Core>,
                                        public EventHandler {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
       scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
       LocalInputMonitor::PointerMoveCallback on_mouse_move);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  void Start();
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<Core>;
  ~Core() override;

  void StartOnUiThread();
  void StopOnUiThread();

  // EventHandler interface.
  void OnLocalPointerEvent(const webrtc::DesktopVector& position,
                           ui::EventType type) override;

  // Task runner on which public methods of this class must be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner on which |window_| is created.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  LocalInputMonitorManager* __strong manager_ = nil;

  // Invoked in the |caller_task_runner_| thread to report local mouse events.
  LocalInputMonitor::PointerMoveCallback on_mouse_move_;

  webrtc::DesktopVector mouse_position_;
};

LocalMouseInputMonitorMac::LocalMouseInputMonitorMac(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::PointerMoveCallback on_mouse_move)
    : core_(new Core(caller_task_runner,
                     ui_task_runner,
                     std::move(on_mouse_move))) {
  core_->Start();
}

LocalMouseInputMonitorMac::~LocalMouseInputMonitorMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  core_->Stop();
}

LocalMouseInputMonitorMac::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::PointerMoveCallback on_mouse_move)
    : caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      on_mouse_move_(std::move(on_mouse_move)) {}

void LocalMouseInputMonitorMac::Core::Start() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&Core::StartOnUiThread, this));
}

void LocalMouseInputMonitorMac::Core::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&Core::StopOnUiThread, this));
}

LocalMouseInputMonitorMac::Core::~Core() {
  DCHECK(manager_ == nil);
}

void LocalMouseInputMonitorMac::Core::StartOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  manager_ = [[LocalInputMonitorManager alloc] initWithMonitor:this];
}

void LocalMouseInputMonitorMac::Core::StopOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  [manager_ invalidate];
  manager_ = nil;
}

void LocalMouseInputMonitorMac::Core::OnLocalPointerEvent(
    const webrtc::DesktopVector& position,
    ui::EventType type) {
  // In some cases OS may emit bogus mouse-move events even when cursor is not
  // actually moving. To handle this case properly verify that mouse position
  // has changed for move events. See https://crbug.com/360912.
  if (type == ui::EventType::kMouseMoved && position.equals(mouse_position_)) {
    return;
  }

  mouse_position_ = position;

  caller_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(on_mouse_move_, position, type));
}

}  // namespace

std::unique_ptr<LocalPointerInputMonitor> LocalPointerInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::PointerMoveCallback on_mouse_move,
    base::OnceClosure disconnect_callback) {
  return std::make_unique<LocalMouseInputMonitorMac>(
      caller_task_runner, ui_task_runner, std::move(on_mouse_move));
}

}  // namespace remoting
