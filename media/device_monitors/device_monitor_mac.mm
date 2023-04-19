// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/device_monitors/device_monitor_mac.h"

#include <AVFoundation/AVFoundation.h>

#include <set>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#import "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "media/base/mac/video_capture_device_avfoundation_helpers.h"

namespace {

// This class is used to keep track of system devices names and their types.
class DeviceInfo {
 public:
  enum DeviceType { kAudio, kVideo, kMuxed, kUnknown, kInvalid };

  DeviceInfo(const std::string& unique_id, DeviceType type)
      : unique_id_(unique_id), type_(type) {}

  // Operator== is needed here to use this class in a std::find. A given
  // |unique_id_| always has the same |type_| so for comparison purposes the
  // latter can be safely ignored.
  bool operator==(const DeviceInfo& device) const {
    return unique_id_ == device.unique_id_;
  }

  const std::string& unique_id() const { return unique_id_; }
  DeviceType type() const { return type_; }

 private:
  std::string unique_id_;
  DeviceType type_;
  // Allow generated copy constructor and assignment.
};

// Base abstract class used by DeviceMonitorMac.
class DeviceMonitorMacImpl {
 public:
  explicit DeviceMonitorMacImpl(media::DeviceMonitorMac* monitor)
      : monitor_(monitor) {
    DCHECK(monitor);
    // Initialise the devices_cache_ with a not-valid entry. For the case in
    // which there is one single device in the system and we get notified when
    // it gets removed, this will prevent the system from thinking that no
    // devices were added nor removed and not notifying the |monitor_|.
    cached_devices_.push_back(DeviceInfo("invalid", DeviceInfo::kInvalid));
  }

  DeviceMonitorMacImpl(const DeviceMonitorMacImpl&) = delete;
  DeviceMonitorMacImpl& operator=(const DeviceMonitorMacImpl&) = delete;

  virtual ~DeviceMonitorMacImpl() {}

  virtual void OnDeviceChanged() = 0;

  // Method called by the default notification center when a device is removed
  // or added to the system. It will compare the |cached_devices_| with the
  // current situation, update it, and, if there's an update, signal to
  // |monitor_| with the appropriate device type.
  void ConsolidateDevicesListAndNotify(
      const std::vector<DeviceInfo>& snapshot_devices);

  media::DeviceMonitorMac* monitor() const { return monitor_; }

 protected:
  // Handles to NSNotificationCenter block observers.
  id device_arrival_ = nil;
  id device_removal_ = nil;

 private:
  raw_ptr<media::DeviceMonitorMac> monitor_;
  std::vector<DeviceInfo> cached_devices_;
};

void DeviceMonitorMacImpl::ConsolidateDevicesListAndNotify(
    const std::vector<DeviceInfo>& snapshot_devices) {
  bool video_device_added = false;
  bool video_device_removed = false;

  // Compare the current system devices snapshot with the ones cached to detect
  // additions, present in the former but not in the latter. If we find a device
  // in snapshot_devices entry also present in cached_devices, we remove it from
  // the latter vector.
  std::vector<DeviceInfo>::const_iterator it;
  for (it = snapshot_devices.begin(); it != snapshot_devices.end(); ++it) {
    std::vector<DeviceInfo>::iterator cached_devices_iterator =
        base::ranges::find(cached_devices_, *it);
    if (cached_devices_iterator == cached_devices_.end()) {
      video_device_added |= ((it->type() == DeviceInfo::kVideo) ||
                             (it->type() == DeviceInfo::kMuxed));
      DVLOG(1) << "Video device has been added, id: " << it->unique_id();
    } else {
      cached_devices_.erase(cached_devices_iterator);
    }
  }
  // All the remaining entries in cached_devices are removed devices.
  for (it = cached_devices_.begin(); it != cached_devices_.end(); ++it) {
    video_device_removed |= ((it->type() == DeviceInfo::kVideo) ||
                             (it->type() == DeviceInfo::kMuxed) ||
                             (it->type() == DeviceInfo::kInvalid));
    DVLOG(1) << "Video device has been removed, id: " << it->unique_id();
  }
  // Update the cached devices with the current system snapshot.
  cached_devices_ = snapshot_devices;

  if (video_device_added || video_device_removed) {
    monitor_->NotifyDeviceChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
  }
}

// Forward declaration for use by CrAVFoundationDeviceObserver.
class SuspendObserverDelegate;

BASE_FEATURE(kUseAVCaptureDeviceDiscoverySessionDeviceMonitor,
             "UseAVCaptureDeviceDiscoverySessionDeviceMonitor",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, this feature will cache devices in DeviceMonitorMac. In this
// case, MediaDevicesManager::OnDevicesChanged event will only be sent if
// DeviceMonitorMac sees a difference in snapshot devices vs cached devices.
BASE_FEATURE(kCacheResultsInDeviceMontiorMac,
             "CacheResultsInDeviceMontiorMac",
             base::FEATURE_DISABLED_BY_DEFAULT);

NSArray<AVCaptureDevice*>* ListCameras() {
  return media::GetVideoCaptureDevices(base::FeatureList::IsEnabled(
      kUseAVCaptureDeviceDiscoverySessionDeviceMonitor));
}

}  // namespace

// This class is a Key-Value Observer (KVO) shim. It is needed because C++
// classes cannot observe Key-Values directly. Created, manipulated, and
// destroyed on the UI Thread by SuspendObserverDelegate.
@interface CrAVFoundationDeviceObserver : NSObject {
 @private
  // Callback for device changed, has to run on Device Thread.
  base::RepeatingClosure _onDeviceChangedCallback;

  // Member to keep track of the devices we are already monitoring.
  std::set<base::scoped_nsobject<AVCaptureDevice>> _monitoredDevices;

  // Pegged to the "main" thread -- usually content::BrowserThread::UI.
  base::ThreadChecker _mainThreadChecker;
}

- (instancetype)initWithOnChangedCallback:
    (const base::RepeatingClosure&)callback;
- (void)startObserving:(base::scoped_nsobject<AVCaptureDevice>)device;
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
  // Enumerate devices in |device_thread| and run the bottom half in
  // DoOnDeviceChange(). |suspend_observer_| calls back here on suspend event,
  // and our parent AVFoundationMonitorImpl calls on connect/disconnect device.
  void OnDeviceChanged(
      const scoped_refptr<base::SingleThreadTaskRunner>& device_thread);
  // Remove the device monitor's weak reference. Remove ourselves as suspend
  // notification observer from |suspend_observer_|.
  void ResetDeviceMonitor();

 private:
  friend class base::RefCountedThreadSafe<SuspendObserverDelegate>;

  virtual ~SuspendObserverDelegate();

  // Bottom half of StartObserver(), starts |suspend_observer_| for all devices.
  // Assumes that |devices| has been retained prior to being called, and
  // releases it internally.
  void DoStartObserver(NSArray* devices);
  // Bottom half of OnDeviceChanged(), starts |suspend_observer_| for current
  // devices and composes a snapshot of them to send it to
  // |avfoundation_monitor_impl_|. Assumes that |devices| has been retained
  // prior to being called, and releases it internally.
  void DoOnDeviceChanged(NSArray* devices);

  base::scoped_nsobject<CrAVFoundationDeviceObserver> suspend_observer_;
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

  base::RepeatingClosure on_device_changed_callback = base::BindRepeating(
      &SuspendObserverDelegate::OnDeviceChanged, this, device_thread);
  suspend_observer_.reset([[CrAVFoundationDeviceObserver alloc]
      initWithOnChangedCallback:on_device_changed_callback]);

  // Enumerate the devices in Device thread and post the observers start to be
  // done on UI thread. The devices array is retained in |device_thread| and
  // released in DoStartObserver().
  device_thread->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(base::RetainBlock(^{
        return [ListCameras() retain];
      })),
      base::BindOnce(&SuspendObserverDelegate::DoStartObserver, this));
}

void SuspendObserverDelegate::OnDeviceChanged(
    const scoped_refptr<base::SingleThreadTaskRunner>& device_thread) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (base::FeatureList::IsEnabled(kCacheResultsInDeviceMontiorMac)) {
    // Enumerate the devices in Device thread and post the consolidation of the
    // new devices and the old ones to be done on main thread. The devices array
    // is retained in |device_thread| and released in DoOnDeviceChanged().
    device_thread->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(base::RetainBlock(^{
          return [ListCameras() retain];
        })),
        base::BindOnce(&SuspendObserverDelegate::DoOnDeviceChanged, this));
  } else {
    // |avfoundation_monitor_impl_| might have been NULLed asynchronously before
    // arriving at this line.
    if (avfoundation_monitor_impl_) {
      // Forward the event to MediaDevicesManager::OnDevicesChanged, which will
      // enumerate the devices on its own.
      avfoundation_monitor_impl_->monitor()->NotifyDeviceChanged(
          base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
    }
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

void SuspendObserverDelegate::DoStartObserver(NSArray* devices) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::scoped_nsobject<NSArray> auto_release(devices);
  for (AVCaptureDevice* device in devices) {
    base::scoped_nsobject<AVCaptureDevice> device_ptr([device retain]);
    [suspend_observer_ startObserving:device_ptr];
  }
}

void SuspendObserverDelegate::DoOnDeviceChanged(NSArray* devices) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::scoped_nsobject<NSArray> auto_release(devices);
  std::vector<DeviceInfo> snapshot_devices;
  for (AVCaptureDevice* device in devices) {
    base::scoped_nsobject<AVCaptureDevice> device_ptr([device retain]);
    [suspend_observer_ startObserving:device_ptr];

    BOOL suspended = [device isSuspended];
    DeviceInfo::DeviceType device_type = DeviceInfo::kUnknown;
    if ([device hasMediaType:AVMediaTypeVideo]) {
      if (suspended) {
        continue;
      }
      device_type = DeviceInfo::kVideo;
    } else if ([device hasMediaType:AVMediaTypeMuxed]) {
      device_type = suspended ? DeviceInfo::kAudio : DeviceInfo::kMuxed;
    } else if ([device hasMediaType:AVMediaTypeAudio]) {
      device_type = DeviceInfo::kAudio;
    }
    snapshot_devices.push_back(
        DeviceInfo([[device uniqueID] UTF8String], device_type));
  }
  // Make sure no references are held to |devices| when
  // ConsolidateDevicesListAndNotify is called since the VideoCaptureManager
  // and AudioCaptureManagers also enumerates the available devices but on
  // another thread.
  auto_release.reset();
  // |avfoundation_monitor_impl_| might have been NULLed asynchronously before
  // arriving at this line.
  if (avfoundation_monitor_impl_) {
    avfoundation_monitor_impl_->ConsolidateDevicesListAndNotify(
        snapshot_devices);
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
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  device_arrival_ =
      [nc addObserverForName:AVCaptureDeviceWasConnectedNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                    OnDeviceChanged();
                  }];
  device_removal_ =
      [nc addObserverForName:AVCaptureDeviceWasDisconnectedNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                    OnDeviceChanged();
                  }];
  suspend_observer_delegate_->StartObserver(device_task_runner_);
}

AVFoundationMonitorImpl::~AVFoundationMonitorImpl() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  suspend_observer_delegate_->ResetDeviceMonitor();
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:device_arrival_];
  [nc removeObserver:device_removal_];
}

void AVFoundationMonitorImpl::OnDeviceChanged() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  suspend_observer_delegate_->OnDeviceChanged(device_task_runner_);
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
  std::set<base::scoped_nsobject<AVCaptureDevice>>::iterator it =
      _monitoredDevices.begin();
  while (it != _monitoredDevices.end()) {
    [self removeObservers:*(it++)];
  }
  [super dealloc];
}

- (void)startObserving:(base::scoped_nsobject<AVCaptureDevice>)device {
  DCHECK(_mainThreadChecker.CalledOnValidThread());
  DCHECK(device != nil);
  // Skip this device if there are already observers connected to it.
  if (base::Contains(_monitoredDevices, device)) {
    return;
  }
  [device addObserver:self
           forKeyPath:@"suspended"
              options:0
              context:device.get()];
  [device addObserver:self
           forKeyPath:@"connected"
              options:0
              context:device.get()];
  _monitoredDevices.insert(device);
}

- (void)stopObserving:(AVCaptureDevice*)device {
  DCHECK(_mainThreadChecker.CalledOnValidThread());
  DCHECK(device != nil);

  std::set<base::scoped_nsobject<AVCaptureDevice>>::iterator found =
      base::ranges::find(_monitoredDevices, device);
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
  if ([device observationInfo]) {
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
    [self stopObserving:static_cast<AVCaptureDevice*>(context)];
  }
}

@end  // @implementation CrAVFoundationDeviceObserver

namespace media {

DeviceMonitorMac::DeviceMonitorMac(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner)
    : device_task_runner_(std::move(device_task_runner)) {
  // AVFoundation do not need to be fired up until the user
  // exercises a GetUserMedia. Bringing up either library and enumerating the
  // devices in the system is an operation taking in the range of hundred of ms,
  // so it is triggered explicitly from MediaStreamManager::StartMonitoring().
}

DeviceMonitorMac::~DeviceMonitorMac() {}

void DeviceMonitorMac::StartMonitoring() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Monitoring via AVFoundation";
  device_monitor_impl_ =
      std::make_unique<AVFoundationMonitorImpl>(this, device_task_runner_);
}

void DeviceMonitorMac::NotifyDeviceChanged(
    base::SystemMonitor::DeviceType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(xians): Remove the global variable for SystemMonitor.
  base::SystemMonitor::Get()->ProcessDevicesChanged(type);
}

}  // namespace media
