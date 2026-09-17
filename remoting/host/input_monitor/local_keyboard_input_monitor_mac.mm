// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_keyboard_input_monitor.h"

#import <AppKit/AppKit.h>
#include <unistd.h>

#include <memory>
#include <utility>

#include "base/apple/scoped_cftyperef.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {
namespace {

class LocalKeyboardInputMonitorMac : public LocalKeyboardInputMonitor {
 public:
  // Invoked by LocalKeyboardInputMonitorManager.
  class EventHandler {
   public:
    virtual ~EventHandler() = default;

    virtual void OnLocalKeyPressed(uint32_t usb_keycode) = 0;
  };

  LocalKeyboardInputMonitorMac(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      LocalInputMonitor::KeyPressedCallback on_key_event_callback,
      base::OnceClosure disconnect_callback);

  LocalKeyboardInputMonitorMac(const LocalKeyboardInputMonitorMac&) = delete;
  LocalKeyboardInputMonitorMac& operator=(const LocalKeyboardInputMonitorMac&) =
      delete;

  ~LocalKeyboardInputMonitorMac() override;

 private:
  // The actual implementation resides in LocalKeyboardInputMonitorMac::Core.
  class Core;
  scoped_refptr<Core> core_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace
}  // namespace remoting

@interface LocalKeyboardInputMonitorManager : NSObject {
 @private
  base::apple::ScopedCFTypeRef<CFRunLoopSourceRef> _keyboardRunLoopSource;
  base::apple::ScopedCFTypeRef<CFMachPortRef> _keyboardMachPort;
  raw_ptr<remoting::LocalKeyboardInputMonitorMac::EventHandler> _monitor;
}

- (instancetype)initWithMonitor:
    (remoting::LocalKeyboardInputMonitorMac::EventHandler*)monitor;

// Called when a local key event is detected.
- (void)localKeyPressed:(uint32_t)usb_keycode;

// Re-enables the event tap if disabled by timeout.
- (void)reEnableEventTap;

// Must be called when the LocalKeyboardInputMonitorManager is no longer needed.
- (void)invalidate;

@end

static CGEventRef LocalKeyboardEvent([[maybe_unused]] CGEventTapProxy proxy,
                                     CGEventType type,
                                     CGEventRef event,
                                     void* context) {
  LocalKeyboardInputMonitorManager* manager =
      (__bridge LocalKeyboardInputMonitorManager*)context;
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
    int mac_keycode = static_cast<int>(
        CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
    uint32_t usb_keycode =
        ui::KeycodeConverter::NativeKeycodeToUsbKeycode(mac_keycode);
    [manager localKeyPressed:usb_keycode];
  }
  return event;
}

@implementation LocalKeyboardInputMonitorManager

- (instancetype)initWithMonitor:
    (remoting::LocalKeyboardInputMonitorMac::EventHandler*)monitor {
  if ((self = [super init])) {
    _monitor = monitor;

    const CGEventMask keyboard_mask = CGEventMaskBit(kCGEventKeyDown) |
                                      CGEventMaskBit(kCGEventKeyUp) |
                                      CGEventMaskBit(kCGEventFlagsChanged);

    _keyboardMachPort.reset(CGEventTapCreate(
        kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly,
        keyboard_mask, LocalKeyboardEvent, (__bridge void*)self));
    if (_keyboardMachPort) {
      _keyboardRunLoopSource.reset(
          CFMachPortCreateRunLoopSource(nullptr, _keyboardMachPort.get(), 0));
      CFRunLoopAddSource(CFRunLoopGetMain(), _keyboardRunLoopSource.get(),
                         kCFRunLoopCommonModes);
    } else {
      LOG(ERROR) << "CGEventTapCreate failed for keyboard.";
      self = nil;
      return nil;
    }
  }
  return self;
}

- (void)localKeyPressed:(uint32_t)usb_keycode {
  _monitor->OnLocalKeyPressed(usb_keycode);
}

- (void)reEnableEventTap {
  if (_keyboardMachPort) {
    CGEventTapEnable(_keyboardMachPort.get(), true);
  }
}

- (void)invalidate {
  if (_keyboardRunLoopSource) {
    CFMachPortInvalidate(_keyboardMachPort.get());
    CFRunLoopRemoveSource(CFRunLoopGetMain(), _keyboardRunLoopSource.get(),
                          kCFRunLoopCommonModes);
    _keyboardMachPort.reset();
    _keyboardRunLoopSource.reset();
  }
}

- (void)dealloc {
  [self invalidate];
}

@end

namespace remoting {
namespace {

class LocalKeyboardInputMonitorMac::Core
    : public base::RefCountedThreadSafe<Core>,
      public EventHandler {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
       scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
       LocalInputMonitor::KeyPressedCallback on_key_event_callback,
       base::OnceClosure disconnect_callback);

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
  void OnLocalKeyPressed(uint32_t usb_keycode) override;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  LocalKeyboardInputMonitorManager* __strong manager_ = nil;

  LocalInputMonitor::KeyPressedCallback on_key_event_callback_;
  base::OnceClosure disconnect_callback_;
};

LocalKeyboardInputMonitorMac::LocalKeyboardInputMonitorMac(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::KeyPressedCallback on_key_event_callback,
    base::OnceClosure disconnect_callback)
    : core_(base::MakeRefCounted<Core>(std::move(caller_task_runner),
                                       std::move(ui_task_runner),
                                       std::move(on_key_event_callback),
                                       std::move(disconnect_callback))) {
  core_->Start();
}

LocalKeyboardInputMonitorMac::~LocalKeyboardInputMonitorMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  core_->Stop();
}

LocalKeyboardInputMonitorMac::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::KeyPressedCallback on_key_event_callback,
    base::OnceClosure disconnect_callback)
    : ui_task_runner_(std::move(ui_task_runner)),
      on_key_event_callback_(
          base::BindPostTask(caller_task_runner,
                             std::move(on_key_event_callback))),
      disconnect_callback_(
          disconnect_callback
              ? base::BindPostTask(caller_task_runner,
                                   std::move(disconnect_callback))
              : base::OnceClosure()) {}

void LocalKeyboardInputMonitorMac::Core::Start() {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::StartOnUiThread, base::WrapRefCounted(this)));
}

void LocalKeyboardInputMonitorMac::Core::Stop() {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::StopOnUiThread, base::WrapRefCounted(this)));
}

LocalKeyboardInputMonitorMac::Core::~Core() = default;

void LocalKeyboardInputMonitorMac::Core::StartOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  manager_ = [[LocalKeyboardInputMonitorManager alloc] initWithMonitor:this];
  if (!manager_ && disconnect_callback_) {
    std::move(disconnect_callback_).Run();
  }
}

void LocalKeyboardInputMonitorMac::Core::StopOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  [manager_ invalidate];
  manager_ = nil;
  disconnect_callback_.Reset();
}

void LocalKeyboardInputMonitorMac::Core::OnLocalKeyPressed(
    uint32_t usb_keycode) {
  on_key_event_callback_.Run(usb_keycode);
}

}  // namespace

std::unique_ptr<LocalKeyboardInputMonitor> LocalKeyboardInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> /* input_task_runner */,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::KeyPressedCallback on_key_event_callback,
    base::OnceClosure disconnect_callback) {
  return std::make_unique<LocalKeyboardInputMonitorMac>(
      std::move(caller_task_runner), std::move(ui_task_runner),
      std::move(on_key_event_callback), std::move(disconnect_callback));
}

}  // namespace remoting
