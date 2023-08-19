// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_hotkey_input_monitor.h"

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>

#include <cstdint>
#include <utility>

#include "base/apple/scoped_cftyperef.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {
namespace {

class LocalHotkeyInputMonitorMac : public LocalHotkeyInputMonitor {
 public:
  // Invoked by LocalHotkeyInputMonitorManager.
  class EventHandler {
   public:
    virtual ~EventHandler() = default;

    virtual void OnDisconnectShortcut() = 0;
  };

  LocalHotkeyInputMonitorMac(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::OnceClosure disconnect_callback);

  LocalHotkeyInputMonitorMac(const LocalHotkeyInputMonitorMac&) = delete;
  LocalHotkeyInputMonitorMac& operator=(const LocalHotkeyInputMonitorMac&) =
      delete;

  ~LocalHotkeyInputMonitorMac() override;

 private:
  // The implementation resides in LocalHotkeyInputMonitorMac::Core class.
  class Core;
  scoped_refptr<Core> core_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace
}  // namespace remoting

@interface LocalHotkeyInputMonitorManager : NSObject

- (instancetype)initWithMonitor:
    (remoting::LocalHotkeyInputMonitorMac::EventHandler*)monitor;

@end

@implementation LocalHotkeyInputMonitorManager {
  id __strong _eventMonitor;

  raw_ptr<remoting::LocalHotkeyInputMonitorMac::EventHandler> _monitor;
}

- (instancetype)initWithMonitor:
    (remoting::LocalHotkeyInputMonitorMac::EventHandler*)monitor {
  if ((self = [super init])) {
    _monitor = monitor;

    LocalHotkeyInputMonitorManager* __weak weakSelf = self;
    auto eventHandler = ^NSEvent*(NSEvent* event) {
      LocalHotkeyInputMonitorManager* strongSelf = weakSelf;
      if (!strongSelf) {
        return event;
      }

      const NSEventModifierFlags requiredModifiers =
          NSEventModifierFlagOption | NSEventModifierFlagControl;
      if ((event.keyCode == kVK_Escape) &&
          (event.modifierFlags & requiredModifiers)) {
        // Trigger the callback.
        strongSelf->_monitor->OnDisconnectShortcut();

        // Stop the event propagation.
        return nil;
      }

      // Otherwise, let the event continue propagating.
      return event;
    };

    _eventMonitor =
        [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown
                                              handler:eventHandler];
  }
  return self;
}

- (void)dealloc {
  if (_eventMonitor) {
    [NSEvent removeMonitor:_eventMonitor];
  }
}

@end

namespace remoting {
namespace {

class LocalHotkeyInputMonitorMac::Core
    : public base::RefCountedThreadSafe<Core>,
      public EventHandler {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
       scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
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
  void OnDisconnectShortcut() override;

  // Task runner on which public methods of this class must be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner on which |window_| is created.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  LocalHotkeyInputMonitorManager* __strong manager_;

  // Invoked in the |caller_task_runner_| thread to report session disconnect
  // requests.
  base::OnceClosure disconnect_callback_;
};

LocalHotkeyInputMonitorMac::LocalHotkeyInputMonitorMac(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::OnceClosure disconnect_callback)
    : core_(new Core(caller_task_runner,
                     ui_task_runner,
                     std::move(disconnect_callback))) {
  core_->Start();
}

LocalHotkeyInputMonitorMac::~LocalHotkeyInputMonitorMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  core_->Stop();
}

LocalHotkeyInputMonitorMac::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::OnceClosure disconnect_callback)
    : caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      disconnect_callback_(std::move(disconnect_callback)) {
  DCHECK(disconnect_callback_);
}

void LocalHotkeyInputMonitorMac::Core::Start() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&Core::StartOnUiThread, this));
}

void LocalHotkeyInputMonitorMac::Core::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&Core::StopOnUiThread, this));
}

LocalHotkeyInputMonitorMac::Core::~Core() {
  DCHECK_EQ(manager_, nil);
}

void LocalHotkeyInputMonitorMac::Core::StartOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  manager_ = [[LocalHotkeyInputMonitorManager alloc] initWithMonitor:this];
}

void LocalHotkeyInputMonitorMac::Core::StopOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  manager_ = nil;
}

void LocalHotkeyInputMonitorMac::Core::OnDisconnectShortcut() {
  if (disconnect_callback_) {
    caller_task_runner_->PostTask(FROM_HERE, std::move(disconnect_callback_));
  }
}

}  // namespace

std::unique_ptr<LocalHotkeyInputMonitor> LocalHotkeyInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::OnceClosure disconnect_callback) {
  return std::make_unique<LocalHotkeyInputMonitorMac>(
      caller_task_runner, ui_task_runner, std::move(disconnect_callback));
}

}  // namespace remoting
