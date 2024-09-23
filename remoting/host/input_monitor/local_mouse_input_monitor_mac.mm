// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_pointer_input_monitor.h"

#import <AppKit/AppKit.h>

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
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {
namespace {

// Note that this class does not detect touch input and so is named accordingly.
class LocalMouseInputMonitorMac : public LocalPointerInputMonitor {
 public:
  // Invoked by LocalInputMonitorManager.
  class EventHandler {
   public:
    virtual ~EventHandler() = default;

    virtual void OnLocalMouseMoved(const webrtc::DesktopVector& position) = 0;
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

// Called when the local mouse moves
- (void)localMouseMoved:(const webrtc::DesktopVector&)mousePos;

// Must be called when the LocalInputMonitorManager is no longer to be used.
// Similar to NSTimer in that more than a simple release is required.
- (void)invalidate;

@end

static CGEventRef LocalMouseMoved(CGEventTapProxy proxy,
                                  CGEventType type,
                                  CGEventRef event,
                                  void* context) {
  int64_t pid = CGEventGetIntegerValueField(event, kCGEventSourceUnixProcessID);
  if (pid == 0) {
    CGPoint cgMousePos = CGEventGetLocation(event);
    webrtc::DesktopVector mousePos(cgMousePos.x, cgMousePos.y);
    [(__bridge LocalInputMonitorManager*)context localMouseMoved:mousePos];
  }
  return nullptr;
}

@implementation LocalInputMonitorManager

- (instancetype)initWithMonitor:
    (remoting::LocalMouseInputMonitorMac::EventHandler*)monitor {
  if ((self = [super init])) {
    _monitor = monitor;

    _mouseMachPort.reset(CGEventTapCreate(
        kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly,
        1 << kCGEventMouseMoved, LocalMouseMoved, (__bridge void*)self));
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

- (void)localMouseMoved:(const webrtc::DesktopVector&)mousePos {
  _monitor->OnLocalMouseMoved(mousePos);
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
  void OnLocalMouseMoved(const webrtc::DesktopVector& position) override;

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

void LocalMouseInputMonitorMac::Core::OnLocalMouseMoved(
    const webrtc::DesktopVector& position) {
  // In some cases OS may emit bogus mouse-move events even when cursor is not
  // actually moving. To handle this case properly verify that mouse position
  // has changed. See https://crbug.com/360912.
  if (position.equals(mouse_position_)) {
    return;
  }

  mouse_position_ = position;

  caller_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(on_mouse_move_, position, ui::EventType::kMouseMoved));
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
