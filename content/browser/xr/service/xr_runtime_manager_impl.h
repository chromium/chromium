// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_SERVICE_XR_RUNTIME_MANAGER_IMPL_H_
#define CONTENT_BROWSER_XR_SERVICE_XR_RUNTIME_MANAGER_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/xr/service/browser_xr_runtime_impl.h"
#include "content/browser/xr/service/vr_service_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/xr_integration_client.h"
#include "content/public/browser/xr_runtime_manager.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

namespace content {
class XRRuntimeManagerTest;

// Singleton used to provide the platform's XR Runtimes to VRServiceImpl
// instances.
class CONTENT_EXPORT XRRuntimeManagerImpl
    : public XRRuntimeManager,
      public base::RefCounted<XRRuntimeManagerImpl>,
      public content::GpuDataManagerObserver,
      public device::VRDeviceProviderClient {
 public:
  friend base::RefCounted<XRRuntimeManagerImpl>;

  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  friend XRRuntimeManagerTest;
  XRRuntimeManagerImpl(const XRRuntimeManagerImpl&) = delete;
  XRRuntimeManagerImpl& operator=(const XRRuntimeManagerImpl&) = delete;

  // Returns a pointer to the XRRuntimeManagerImpl singleton.
  // If The singleton is not currently instantiated, this instantiates it with
  // the built-in set of providers.
  // The singleton will persist until all pointers have been dropped.
  static scoped_refptr<XRRuntimeManagerImpl> GetOrCreateInstance();

  // Returns the WebContents currently being displayed in a WebXR Immersive
  // Session, if any, null otherwise.
  static content::WebContents* GetImmersiveSessionWebContents();

  // Adds a listener for runtime manager events. XRRuntimeManagerImpl does not
  // own this object.
  void AddService(VRServiceImpl* service);
  void RemoveService(VRServiceImpl* service);

  BrowserXRRuntimeImpl* GetRuntimeForOptions(
      device::mojom::XRSessionOptions* options);

  // Gets the runtime matching a currently-active immersive session, if any.
  // This is either the VR or AR runtime, or null if there's no matching
  // runtime or if there's no active immersive session.
  BrowserXRRuntimeImpl* GetCurrentlyPresentingImmersiveRuntime();

  // Returns true if another service is presenting. Returns false if this
  // service is presenting, or if nobody is presenting.
  bool IsOtherClientPresenting(VRServiceImpl* service);

  void SupportsSession(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::VRService::SupportsSessionCallback callback);

  void MakeXrCompatible();

  // content::GpuDataManagerObserver
  void OnGpuInfoUpdate() override;

  // XRRuntimeManager implementation
  BrowserXRRuntimeImpl* GetRuntime(device::mojom::XRDeviceId id) override;
  void ForEachRuntime(
      base::RepeatingCallback<void(BrowserXRRuntime*)> fn) override;

  // VRDeviceProviderClient implementation
  void AddRuntime(
      device::mojom::XRDeviceId id,
      device::mojom::XRDeviceDataPtr device_data,
      mojo::PendingRemote<device::mojom::XRRuntime> runtime) override;
  void RemoveRuntime(device::mojom::XRDeviceId id) override;
  void OnProviderInitialized() override;
  device::XrFrameSinkClientFactory GetXrFrameSinkClientFactory() override;

 private:
  // Constructor also used by tests to supply an arbitrary list of providers
  static scoped_refptr<XRRuntimeManagerImpl> CreateInstance(
      XRProviderList providers);

  // Used by tests to check on runtime state.
  device::mojom::XRRuntime* GetRuntimeForTest(device::mojom::XRDeviceId id);

  // Used by tests
  size_t NumberOfConnectedServices();

  explicit XRRuntimeManagerImpl(XRProviderList providers);

  ~XRRuntimeManagerImpl() override;

  void InitializeProviders();
  bool AreAllProvidersInitialized();

  bool IsInitializedOnCompatibleAdapter(BrowserXRRuntimeImpl* runtime);

  // Gets the system default immersive-vr runtime if available.
  BrowserXRRuntimeImpl* GetImmersiveVrRuntime();

  // Gets the system default immersive-ar runtime if available.
  BrowserXRRuntimeImpl* GetImmersiveArRuntime();

  XRProviderList providers_;

  // XRRuntimes are owned by their providers, each correspond to a
  // BrowserXRRuntimeImpl that is owned by XRRuntimeManagerImpl.
  using DeviceRuntimeMap =
      base::small_map<std::map<device::mojom::XRDeviceId,
                               std::unique_ptr<BrowserXRRuntimeImpl>>>;
  DeviceRuntimeMap runtimes_;

  bool providers_initialized_ = false;
  size_t num_initialized_providers_ = 0;

  bool xr_compatible_restarted_gpu_ = false;
#if BUILDFLAG(IS_WIN)
  CHROME_LUID default_gpu_ = {0, 0};
#endif

  std::set<VRServiceImpl*> services_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_SERVICE_XR_RUNTIME_MANAGER_IMPL_H_
