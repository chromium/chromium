// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_PLATFORM_HELPER_H_
#define DEVICE_VR_OPENXR_OPENXR_PLATFORM_HELPER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom-forward.h"
#include "device/vr/vr_export.h"

namespace device {
class OpenXrGraphicsBinding;

// Simple struct containing the values that the platform will actually need to
// create a session. Right now, Android needs the render_process_id and
// render_frame_id to uniquely look up the WebContents and thus retrieve the
// Activity that this session should be associated with, and this value is
// unused on Windows.
struct OpenXrCreateInfo {
  int render_process_id;
  int render_frame_id;
};

// This class exists to help provide an interface for working with OpenXR
// methods that may have different requirements on the different platforms.
// Whether that is additional information that needs to be passed to
// xrCreateSession, or different rules about XrInstance lifetime management.
class DEVICE_VR_EXPORT OpenXrPlatformHelper {
 public:
  using CreateInstanceCallback =
      base::OnceCallback<void(XrResult result, XrInstance)>;

  using PlatformCreateInfoReadyCallback =
      base::OnceCallback<void(void* create_info)>;

  using PlatormInitiatedShutdownCallback = base::OnceClosure;

  // Gets the set of RequiredExtensions that need to be present on the platform.
  static void GetRequiredExtensions(std::vector<const char*>& extensions);

  // Gets the set of extensions to enable if the platform supports them. These
  // are extension methods that we'd like to have, but will not block creation
  // of an XrInstance if they are not available. This could be things like
  // e.g. specific controllers.
  static std::vector<const char*> GetOptionalExtensions();

  virtual ~OpenXrPlatformHelper();

  // Attempt to perform any platform-specific initialization that needs to
  // happen. E.g. on Android we need to call xrInitializeLoaderKHR before we can
  // make any "CreateInstance" or other calls.
  // Must be called before making any calls to e.g. xrCreateInstance.
  bool EnsureInitialized();

  // Creates an OpenXrGraphicsBinding which is responsible for returning the
  // information about the graphics pipeline that is required to create an
  // XrInstance and/or XrSession.
  virtual std::unique_ptr<OpenXrGraphicsBinding> GetGraphicsBinding() = 0;

  // Gets the ExtensionEnumeration which is the list of extensions supported by
  // the platform.
  const OpenXrExtensionEnumeration* GetExtensionEnumeration() const;

  // Gets any platform-specific struct that needs to be appended to
  // `XrInstanceCreateInfo`.`next`.
  virtual void GetPlatformCreateInfo(
      const device::OpenXrCreateInfo& create_info,
      PlatformCreateInfoReadyCallback result_callback,
      PlatormInitiatedShutdownCallback shutdown_callback) = 0;

  // Used to create an XrInstance. As the different platforms may have
  // different lifetime requirements, xrCreateInstance should only be called via
  // the methods on this class, and the same is true for xrDestroyInstance.
  // Only one "outstanding" XrInstance is allowed at a time.
  virtual XrResult CreateInstance(XrInstance* instance, void* create_info);

  // Convenience method for the above without any OpenXrCreateInfo. Platforms
  // that require additional information via this mechanism will fail creation.
  XrResult CreateInstance(XrInstance* instance);

  void CreateInstanceWithCreateInfo(
      std::optional<OpenXrCreateInfo> create_info,
      CreateInstanceCallback instance_ready_callback,
      PlatormInitiatedShutdownCallback shutdown_callback);

  // Destroys the instance and sets it to XR_NULL_HANDLE on success. As the
  // different platforms may have different lifetime requirements, this should
  // be used in place of directly calling xrDestroyInstance.
  virtual XrResult DestroyInstance(XrInstance& instance);

  // Returns a XRDeviceData prepopulated with everything *except* for supported
  // features.
  virtual device::mojom::XRDeviceData GetXRDeviceData() = 0;

  bool IsArBlendModeSupported(XrInstance instance);

 protected:
  OpenXrPlatformHelper();

  // Perform any platform-specific initialization beyond the default
  // initialization. Returns whether or not initialization succeeded, if it did,
  // then it will not be called again. Will be the first call from
  // EnsureInitialized if not currently initialized.
  virtual bool Initialize() = 0;

  void OnPlatformCreateInfoResult(CreateInstanceCallback callback,
                                  void* instance_create_info);

  // Called when XrCreateInstance fails in order to provide a mechanism to clean
  // up any state that was established prior to the call, since any external
  // cleanup likely won't happen since we don't have an XrInstance.
  virtual void OnInstanceCreateFailure() {}

  XrInstance xr_instance_ = XR_NULL_HANDLE;
  std::unique_ptr<OpenXrExtensionEnumeration> extension_enumeration_;

 private:
  void UpdateExtensionFactorySupport();

  bool initialized_ = false;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_PLATFORM_HELPER_H_
