// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_EXTENSION_HANDLER_FACTORY_H_
#define DEVICE_VR_OPENXR_OPENXR_EXTENSION_HANDLER_FACTORY_H_

#include <memory>
#include <set>

#include "base/containers/flat_set.h"
#include "device/vr/public/mojom/xr_session.mojom-forward.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrAnchorManager;
class OpenXrDepthSensor;
class OpenXrExtensionEnumeration;
class OpenXrExtensionHelper;
class OpenXrHandTracker;
enum class OpenXrHandednessType;
class OpenXrLightEstimator;
class OpenXRSceneUnderstandingManager;
class OpenXrStageBoundsProvider;
class OpenXrUnboundedSpaceProvider;

// The goal of this class is to serve as a base class for factories of our
// various "OpenXrExtensionHandlers". Note that there is no base class for the
// handlers themselves, because they all have very distinct constructor needs
// and callers need to know the actual type that they represent, to query/assign
// the appropriate data to the runtime. By creating this base class, we provide
// a cleaner way to plug into the `OpenXrPlatformHelper` when it is determining
// what features it needs to enable on the runtime, and also a more agnostic way
// of determining the browser process which WebXR features the runtime can
// support. Unfortunately, the lack of a shared "OpenXrExtensionHandler" base
// class, does result in some repetition in the methods available in this
// factory, but as previously mentioned this seems unavoidable.
// This factory provides a "default" implementation for each "Handler" type by
// indicating that it is unsupported and not returning any handler if one is
// requested. Child classes then only need to override the specific
// "IsFooSupported"/"CreateFoo" pair for the handler that they represent, as
// well as the general methods to list their required extensions and indicate
// the webxr feature(s) that they support.
class OpenXrExtensionHandlerFactory {
 public:
  OpenXrExtensionHandlerFactory();
  virtual ~OpenXrExtensionHandlerFactory();

  // Returns the list of extensions that the handler to be created would want
  // enabled by the runtime if they are present. Note that some handlers may
  // require all of these extensions to be enabled to be created, while others
  // may only require that some are created.
  virtual const base::flat_set<std::string_view>& GetRequestedExtensions()
      const = 0;

  // Returns the list of `XRSessionFeatures` that the handler to be created
  // can support based on the given set of enabled extensions.
  // Uses a std::set instead of a base::flat_set, even though we expect the size
  // to be similar to the above, because implementations may need to dynamically
  // populate this list, unlike the list of extensions which are expected to
  // essentially be static, as such we do not want to enforce any data types
  // (or const& values) on this function's implementations.
  virtual std::set<device::mojom::XRSessionFeature> GetSupportedFeatures(
      const OpenXrExtensionEnumeration* extension_enum) const = 0;

  // Returns whether or not this factory is enabled (e.g. can return at least
  // one "ExtensionHandler" type). By default checks if all RequestedExtensions
  // are supported.
  virtual bool IsEnabled(
      const OpenXrExtensionEnumeration* extension_enum) const;

  // All currently supported extensions that want to query system properties
  // have their own struct defined as a forced leaf-node, so all extensions that
  // want to query xrGetSystemProperties must make the call themselves. This
  // will be called during initialization.
  virtual void ProcessSystemProperties(
      const OpenXrExtensionEnumeration* extension_enum,
      XrInstance instance,
      XrSystemId system);

  virtual std::unique_ptr<OpenXrAnchorManager> CreateAnchorManager(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session,
      XrSpace mojo_space) const;

  virtual std::unique_ptr<OpenXrDepthSensor> CreateDepthSensor(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session,
      XrSpace mojo_space,
      const mojom::XRDepthOptions& depth_options) const;

  virtual std::unique_ptr<OpenXrHandTracker> CreateHandTracker(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session,
      OpenXrHandednessType type) const;

  virtual std::unique_ptr<OpenXrLightEstimator> CreateLightEstimator(
      const OpenXrExtensionHelper& extenion_helper,
      XrSession session,
      XrSpace local_space) const;

  virtual std::unique_ptr<OpenXRSceneUnderstandingManager>
  CreateSceneUnderstandingManager(const OpenXrExtensionHelper& extension_helper,
                                  XrSession session,
                                  XrSpace mojo_space) const;

  virtual std::unique_ptr<OpenXrStageBoundsProvider> CreateStageBoundsProvider(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session) const;

  virtual std::unique_ptr<OpenXrUnboundedSpaceProvider>
  CreateUnboundedSpaceProvider(
      const OpenXrExtensionHelper& extension_helper) const;

 protected:
  bool AreAllRequestedExtensionsSupported(
      const OpenXrExtensionEnumeration* extension_enum) const;

  void SetSystemPropertiesSupport(bool supported);

 private:
  bool supported_by_system_properties_ = false;
};
}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_EXTENSION_HANDLER_FACTORY_H_
