/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_NAVIGATOR_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_NAVIGATOR_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/navigator_concurrent_hardware.h"
#include "third_party/blink/renderer/core/frame/navigator_device_memory.h"
#include "third_party/blink/renderer/core/frame/navigator_id.h"
#include "third_party/blink/renderer/core/frame/navigator_language.h"
#include "third_party/blink/renderer/core/frame/navigator_on_line.h"
#include "third_party/blink/renderer/core/frame/navigator_ua.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/forward_declared_member.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ContextLifecycleObserver;
class GPU;
class Geolocation;
class HID;
class LockManager;
class LockedMode;
class MediaCapabilities;
class NavigatorML;
class NavigatorStorageQuota;
class NetworkInformation;
class Permissions;
class Serial;
class SmartCardResourceManager;
class StorageBucketManager;
class USB;
class WakeLock;

template <typename T>
class GlobalFetchImpl;

// NavigatorBase is a helper for shared logic between Navigator and
// WorkerNavigator.
class CORE_EXPORT NavigatorBase : public ScriptWrappable,
                                  public NavigatorConcurrentHardware,
                                  public NavigatorDeviceMemory,
                                  public NavigatorID,
                                  public NavigatorLanguage,
                                  public NavigatorOnLine,
                                  public NavigatorUA,
                                  public ExecutionContextClient {
 public:
  explicit NavigatorBase(ExecutionContext* context);

  // NavigatorID override
  String userAgent() const override;
  String platform() const override;
  void Trace(Visitor* visitor) const override;

  unsigned int hardwareConcurrency() const override;

  ForwardDeclaredMember<GlobalFetchImpl<NavigatorBase>> GetGlobalFetchImpl()
      const {
    return global_fetch_impl_;
  }
  void SetGlobalFetchImpl(
      ForwardDeclaredMember<GlobalFetchImpl<NavigatorBase>> global_fetch_impl) {
    global_fetch_impl_ = global_fetch_impl;
  }

  Geolocation* GetGeolocation() const { return geolocation_; }
  void SetGeolocation(Geolocation* geolocation) { geolocation_ = geolocation; }

  ForwardDeclaredMember<GPU> GetGPU() const { return gpu_; }
  void SetGPU(ForwardDeclaredMember<GPU> gpu) { gpu_ = gpu; }

  ForwardDeclaredMember<HID> GetHID() const { return hid_; }
  void SetHID(ForwardDeclaredMember<HID> hid) { hid_ = hid; }

  ForwardDeclaredMember<LockManager> GetLockManager() const {
    return lock_manager_;
  }
  void SetLockManager(ForwardDeclaredMember<LockManager> lock_manager) {
    lock_manager_ = lock_manager;
  }

  ForwardDeclaredMember<LockedMode> GetLockedMode() const {
    return locked_mode_;
  }
  void SetLockedMode(ForwardDeclaredMember<LockedMode> locked_mode) {
    locked_mode_ = locked_mode;
  }

  ForwardDeclaredMember<MediaCapabilities> GetMediaCapabilities() const {
    return media_capabilities_;
  }
  void SetMediaCapabilities(
      ForwardDeclaredMember<MediaCapabilities> media_capabilities) {
    media_capabilities_ = media_capabilities;
  }

  ForwardDeclaredMember<NavigatorML> GetNavigatorML() const {
    return navigator_ml_;
  }
  void SetNavigatorML(ForwardDeclaredMember<NavigatorML> navigator_ml) {
    navigator_ml_ = navigator_ml;
  }

  ForwardDeclaredMember<NavigatorStorageQuota> GetNavigatorStorageQuota()
      const {
    return navigator_storage_quota_;
  }
  void SetNavigatorStorageQuota(
      ForwardDeclaredMember<NavigatorStorageQuota> navigator_storage_quota) {
    navigator_storage_quota_ = navigator_storage_quota;
  }

  ForwardDeclaredMember<NetworkInformation, ContextLifecycleObserver>
  GetNetworkInformation() const {
    return network_information_;
  }
  void SetNetworkInformation(
      ForwardDeclaredMember<NetworkInformation, ContextLifecycleObserver>
          network_information) {
    network_information_ = network_information;
  }

  ForwardDeclaredMember<Permissions> GetPermissions() const {
    return permissions_;
  }
  void SetPermissions(ForwardDeclaredMember<Permissions> permissions) {
    permissions_ = permissions;
  }

  ForwardDeclaredMember<Serial> GetSerial() const { return serial_; }
  void SetSerial(ForwardDeclaredMember<Serial> serial) { serial_ = serial; }

  ForwardDeclaredMember<SmartCardResourceManager> GetSmartCardResourceManager()
      const {
    return smart_card_resource_manager_;
  }
  void SetSmartCardResourceManager(
      ForwardDeclaredMember<SmartCardResourceManager>
          smart_card_resource_manager) {
    smart_card_resource_manager_ = smart_card_resource_manager;
  }

  ForwardDeclaredMember<StorageBucketManager> GetStorageBucketManager() const {
    return storage_bucket_manager_;
  }
  void SetStorageBucketManager(
      ForwardDeclaredMember<StorageBucketManager> storage_bucket_manager) {
    storage_bucket_manager_ = storage_bucket_manager;
  }

  ForwardDeclaredMember<USB> GetUSB() const { return usb_; }
  void SetUSB(ForwardDeclaredMember<USB> usb) { usb_ = usb; }

  ForwardDeclaredMember<WakeLock, ContextLifecycleObserver> GetWakeLock()
      const {
    return wake_lock_;
  }
  void SetWakeLock(
      ForwardDeclaredMember<WakeLock, ContextLifecycleObserver> wake_lock) {
    wake_lock_ = wake_lock;
  }

 protected:
  ExecutionContext* GetUAExecutionContext() const override;
  UserAgentMetadata GetUserAgentMetadata() const override;

  Member<Geolocation> geolocation_;
  ForwardDeclaredMember<GlobalFetchImpl<NavigatorBase>> global_fetch_impl_;
  ForwardDeclaredMember<GPU> gpu_;
  ForwardDeclaredMember<HID> hid_;
  ForwardDeclaredMember<LockManager> lock_manager_;
  ForwardDeclaredMember<LockedMode> locked_mode_;
  ForwardDeclaredMember<MediaCapabilities> media_capabilities_;
  ForwardDeclaredMember<NavigatorML> navigator_ml_;
  ForwardDeclaredMember<NavigatorStorageQuota> navigator_storage_quota_;
  ForwardDeclaredMember<NetworkInformation, ContextLifecycleObserver>
      network_information_;
  ForwardDeclaredMember<Permissions> permissions_;
  ForwardDeclaredMember<Serial> serial_;
  ForwardDeclaredMember<SmartCardResourceManager> smart_card_resource_manager_;
  ForwardDeclaredMember<StorageBucketManager> storage_bucket_manager_;
  ForwardDeclaredMember<USB> usb_;
  ForwardDeclaredMember<WakeLock, ContextLifecycleObserver> wake_lock_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_NAVIGATOR_BASE_H_
