// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/device_monitors/device_monitor_mac.h"

#include <AVFoundation/AVFoundation.h>
#include <CoreFoundation/CoreFoundation.h>

#include <set>
#include <string>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "media/base/mac/video_capture_device_avfoundation_helpers.h"

namespace {

// Base abstract class used by DeviceMonitorMac.
class DeviceMonitorMacImpl {
 public:
  explicit DeviceMonitorMacImpl(media::DeviceMonitorMac* monitor)
      : monitor_(monitor) {
    DCHECK(monitor);
  }

  DeviceMonitorMacImpl(const DeviceMonitorMacImpl&) = delete;
  DeviceMonitorMacImpl& operator=(const DeviceMonitorMacImpl&) = delete;

  virtual ~DeviceMonitorMacImpl() = default;

  virtual void OnDeviceChanged() = 0;

  media::DeviceMonitorMac* monitor() const { return monitor_; }

 protected:
  // Handles to NSNotificationCenter block observers.
  id __strong device_arrival_ = nil;
  id __strong device_removal_ = nil;

 private:
  raw_ptr<media::DeviceMonitorMac> monitor_;
};

// Forward declaration for use by CrAVFoundationDeviceObserver.
class SuspendObserverDelegate;

}  // namespace

// This class is a Key-Value Observer (KVO) shim. It is needed because C++
// classes cannot observe Key-Values directly. Created, manipulated, and
// destroyed on the UI Thread by SuspendObserverDelegate.
@interface CrAVFoundationDeviceObserver : NSObject {
 @private
  // Callback for device changed, has to run on Device Thread.
  base::RepeatingClosure _onDeviceChangedCallback;

  // Member to keep track of the devices we are already monitoring.
  std::set<AVCaptureDevice * __strong> _monitoredDevices;

  // Pegged to the "main" thread -- usually content::BrowserThread::UI.
  base::ThreadChecker _mainThreadChecker;
}

- (instancetype)initWithOnChangedCallback:
    (const base::RepeatingClosure&)callback;
- (void)startObserving:(AVCaptureDevice*)device;
- (void)stopObserving:(AVCaptureDevice*)device;
- (void)clearOnDeviceChangedCallback;

@end

namespace {

// This class owns and manages the lifetime of a CrAVFoundationDeviceObserver.
// It is created and destroyed on AVFoundationMonitorImpl's main thread (usually
// browser's UI thread), and it operates on this thread except for the expensive
// device enumerations which are run on Device Thread.
class SuspendObserverDelegate
    : public base::RefCountedThreadSafe<SuspendObserverDelegate> {
 public:
  explicit SuspendObserverDelegate(DeviceMonitorMacImpl* monitor);

  // Create |suspend_observer_| for all devices and register OnDeviceChanged()
  // as its change callback. Schedule bottom half in DoStartObserver().
  void StartObserver(
      const scoped_refptr<base::SingleThreadTaskRunner>& device_thread);
  // Send a notification of devices being changed.
  void OnDeviceChanged();
  // Remove the device monitor's weak reference. Remove ourselves as suspend
  // notification observer from |suspend_observer_|.
  void ResetDeviceMonitor();

 private:
  friend class base::RefCountedThreadSafe<SuspendObserverDelegate>;

  virtual ~SuspendObserverDelegate();

  // Bottom half of StartObserver(), starts |suspend_observer_| for all devices.
  // Assumes that |devices| has been retained prior to being called, and
  // releases it internally.
  void DoStartObserver(NSArray<AVCaptureDevice*>* devices);

  CrAVFoundationDeviceObserver* __strong suspend_observer_;
  raw_ptr<DeviceMonitorMacImpl> avfoundation_monitor_impl_;

  // Pegged to the "main" thread -- usually content::BrowserThread::UI.
  base::ThreadChecker main_thread_checker_;
};

SuspendObserverDelegate::SuspendObserverDelegate(DeviceMonitorMacImpl* monitor)
    : avfoundation_monitor_impl_(monitor) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
}

void SuspendObserverDelegate::StartObserver(
    const scoped_refptr<base::SingleThreadTaskRunner>& device_thread) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  base::RepeatingClosure on_device_changed_callback =
      base::BindRepeating(&SuspendObserverDelegate::OnDeviceChanged, this);
  suspend_observer_ = [[CrAVFoundationDeviceObserver alloc]
      initWithOnChangedCallback:on_device_changed_callback];

  // Enumerate the devices in Device thread and post the observers start to be
  // done on UI thread. The block is bound as a type returning a strong
  // reference which keeps the devices array alive.
  device_thread->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(^{
        return media::GetVideoCaptureDevices();
      }),
      base::BindOnce(&SuspendObserverDelegate::DoStartObserver, this));
}

void SuspendObserverDelegate::OnDeviceChanged() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // |avfoundation_monitor_impl_| might have been NULLed asynchronously before
  // arriving at this line.
  if (avfoundation_monitor_impl_) {
    // Forward the event to MediaDevicesManager::OnDevicesChanged, which will
    // enumerate the devices on its own.
    avfoundation_monitor_impl_->monitor()->NotifyDeviceChanged();
  }
}

void SuspendObserverDelegate::ResetDeviceMonitor() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  avfoundation_monitor_impl_ = nullptr;
  [suspend_observer_ clearOnDeviceChangedCallback];
}

SuspendObserverDelegate::~SuspendObserverDelegate() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
}

void SuspendObserverDelegate::DoStartObserver(
    NSArray<AVCaptureDevice*>* devices) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  for (AVCaptureDevice* device in devices) {
    [suspend_observer_ startObserving:device];
  }
}

// AVFoundation implementation of the Mac Device Monitor, registers as a global
// device connect/disconnect observer and plugs suspend/wake up device observers
// per device. This class is created and lives on the main Application thread
// (UI for content). Owns a SuspendObserverDelegate that notifies when a device
// is suspended/resumed.
class AVFoundationMonitorImpl : public DeviceMonitorMacImpl {
 public:
  AVFoundationMonitorImpl(
      media::DeviceMonitorMac* monitor,
      const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner);

  AVFoundationMonitorImpl(const AVFoundationMonitorImpl&) = delete;
  AVFoundationMonitorImpl& operator=(const AVFoundationMonitorImpl&) = delete;

  ~AVFoundationMonitorImpl() override;

  void OnDeviceChanged() override;

 private:
  bool IsAudioDevice(AVCaptureDevice* device);
  // {Video,AudioInput}DeviceManager's "Device" thread task runner used for
  // posting tasks to |suspend_observer_delegate_|;
  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  // Pegged to the "main" thread -- usually content::BrowserThread::UI.
  base::ThreadChecker main_thread_checker_;

  scoped_refptr<SuspendObserverDelegate> suspend_observer_delegate_;
};

AVFoundationMonitorImpl::AVFoundationMonitorImpl(
    media::DeviceMonitorMac* monitor,
    const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner)
    : DeviceMonitorMacImpl(monitor),
      device_task_runner_(device_task_runner),
      suspend_observer_delegate_(new SuspendObserverDelegate(this)) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  NSNotificationCenter* nc = NSNotificationCenter.defaultCenter;
  device_arrival_ =
      [nc addObserverForName:AVCaptureDeviceWasConnectedNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                    if (!IsAudioDevice(notification.object)) {
                      OnDeviceChanged();
                    }
                  }];
  device_removal_ =
      [nc addObserverForName:AVCaptureDeviceWasDisconnectedNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                    if (!IsAudioDevice(notification.object)) {
                      OnDeviceChanged();
                    }
                  }];
  suspend_observer_delegate_->StartObserver(device_task_runner_);
}

AVFoundationMonitorImpl::~AVFoundationMonitorImpl() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  suspend_observer_delegate_->ResetDeviceMonitor();
  NSNotificationCenter* nc = NSNotificationCenter.defaultCenter;
  [nc removeObserver:device_arrival_];
  [nc removeObserver:device_removal_];
}

void AVFoundationMonitorImpl::OnDeviceChanged() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  suspend_observer_delegate_->OnDeviceChanged();
}

bool AVFoundationMonitorImpl::IsAudioDevice(AVCaptureDevice* device) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  return [device hasMediaType:AVMediaTypeAudio];
}

}  // namespace

@implementation CrAVFoundationDeviceObserver

- (instancetype)initWithOnChangedCallback:
    (const base::RepeatingClosure&)callback {
  DCHECK(_mainThreadChecker.CalledOnValidThread());
  if ((self = [super init])) {
    DCHECK(!callback.is_null());
    _onDeviceChangedCallback = callback;
  }
  return self;
}

- (void)dealloc {
  DCHECK(_mainThreadChecker.CalledOnValidThread());
  auto it = _monitoredDevices.begin();
  while (it != _monitoredDevices.end()) {
    [self removeObservers:*(it++)];
  }
}

- (void)startObserving:(AVCaptureDevice*)device {
  DCHECK(_mainThreadChecker.CalledOnValidThread());
  DCHECK(device != nil);
  // Skip this device if there are already observers connected to it.
  if (base::Contains(_monitoredDevices, device)) {
    return;
  }
  // Pass a raw pointer to the device as the context. This is safe because the
  // device is retained in _monitoredDevices for the duration of the
  // observation.
  [device addObserver:self
           forKeyPath:@"suspended"
              options:0
              context:(__bridge void*)device];
  [device addObserver:self
           forKeyPath:@"connected"
              options:0
              context:(__bridge void*)device];
  _monitoredDevices.insert(device);
}

- (void)stopObserving:(AVCaptureDevice*)device {
  DCHECK(_mainThreadChecker.CalledOnValidThread());
  DCHECK(device != nil);

  auto found = base::ranges::find(_monitoredDevices, device);
  DCHECK(found != _monitoredDevices.end());
  [self removeObservers:*found];
  _monitoredDevices.erase(found);
}

- (void)clearOnDeviceChangedCallback {
  DCHECK(_mainThreadChecker.CalledOnValidThread());
  _onDeviceChangedCallback.Reset();
}

- (void)removeObservers:(AVCaptureDevice*)device {
  DCHECK(_mainThreadChecker.CalledOnValidThread());
  // Check sanity of |device| via its -observationInfo. http://crbug.com/371271.
  if (device.observationInfo) {
    [device removeObserver:self forKeyPath:@"suspended"];
    [device removeObserver:self forKeyPath:@"connected"];
  }
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK(_mainThreadChecker.CalledOnValidThread());
  if ([keyPath isEqual:@"suspended"]) {
    _onDeviceChangedCallback.Run();
  }
  if ([keyPath isEqual:@"connected"]) {
    [self stopObserving:(__bridge AVCaptureDevice*)context];
  }
}

@end  // @implementation CrAVFoundationDeviceObserver

namespace media {

DeviceMonitorMac::DeviceMonitorMac(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner)
    : device_task_runner_(std::move(device_task_runner)) {
  // AVFoundation do not need to be fired up until the user exercises a
  // GetUserMedia. Bringing up either library and enumerating the devices in the
  // system is an operation taking in the range of hundred of ms, so it is
  // triggered explicitly from MediaStreamManager::StartMonitoring().
}

DeviceMonitorMac::~DeviceMonitorMac() = default;

void DeviceMonitorMac::StartMonitoring() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Monitoring via AVFoundation";
  device_monitor_impl_ =
      std::make_unique<AVFoundationMonitorImpl>(this, device_task_runner_);
}

void DeviceMonitorMac::NotifyDeviceChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::SystemMonitor::Get()->ProcessDevicesChanged(
      base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
}

}  // namespace media
