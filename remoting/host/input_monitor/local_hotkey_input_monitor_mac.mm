// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_hotkey_input_monitor.h"

#include "base/memory/raw_ptr.h"

#import <AppKit/AppKit.h>

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMCarbonEvent.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

// Esc Key Code is 53.
// http://boredzo.org/blog/wp-content/uploads/2007/05/IMTx-virtual-keycodes.pdf
static const NSUInteger kEscKeyCode = 53;

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

@interface LocalHotkeyInputMonitorManager : NSObject {
 @private
  GTMCarbonHotKey* _hotKey;
  raw_ptr<remoting::LocalHotkeyInputMonitorMac::EventHandler> _monitor;
}

- (instancetype)initWithMonitor:
    (remoting::LocalHotkeyInputMonitorMac::EventHandler*)monitor;

// Called when the hotKey is hit.
- (void)hotKeyHit:(GTMCarbonHotKey*)hotKey;

// Must be called when the LocalHotkeyInputMonitorManager is no longer needed.
// Similar to NSTimer in that more than a simple release is required.
- (void)invalidate;

@end

@implementation LocalHotkeyInputMonitorManager

- (instancetype)initWithMonitor:
    (remoting::LocalHotkeyInputMonitorMac::EventHandler*)monitor {
  if ((self = [super init])) {
    _monitor = monitor;

    GTMCarbonEventDispatcherHandler* handler =
        [GTMCarbonEventDispatcherHandler sharedEventDispatcherHandler];
    _hotKey = [handler
        registerHotKey:kEscKeyCode
             modifiers:(NSEventModifierFlagOption | NSEventModifierFlagControl)
                target:self
                action:@selector(hotKeyHit:)
              userInfo:nil
           whenPressed:YES];
    if (!_hotKey) {
      LOG(ERROR) << "registerHotKey failed.";
      [self release];
      return nil;
    }
  }
  return self;
}

- (void)hotKeyHit:(GTMCarbonHotKey*)hotKey {
  _monitor->OnDisconnectShortcut();
}

- (void)invalidate {
  if (_hotKey) {
    GTMCarbonEventDispatcherHandler* handler =
        [GTMCarbonEventDispatcherHandler sharedEventDispatcherHandler];
    [handler unregisterHotKey:_hotKey];
    _hotKey = nullptr;
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

  LocalHotkeyInputMonitorManager* manager_;

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
      manager_(nil),
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

  [manager_ invalidate];
  [manager_ release];
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
