// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_API_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_API_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "chromeos/services/media_perception/public/mojom/media_perception_service.mojom.h"
#include "extensions/common/api/media_perception_private.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"

namespace content {

class RenderFrameHost;

}  //  namespace content

namespace extensions {

class MediaPerceptionAPIDelegate {
 public:
  // Callback for loading a CrOS component. |mount_point| will contain a path to
  // the loaded component, if |success| is true (installation succeeded).
  using LoadCrOSComponentCallback =
      base::OnceCallback<void(bool success, const base::FilePath& mount_point)>;

  using MediaPerceptionRequestHandler = base::RepeatingCallback<void(
      chromeos::media_perception::mojom::MediaPerceptionRequest request)>;

  virtual ~MediaPerceptionAPIDelegate() {}

  // Provides an interface through which a media analytics Chrome OS component
  // from Component Updater can be loaded and mounted on a device.
  virtual void LoadCrOSComponent(
      const api::media_perception_private::ComponentType& type,
      LoadCrOSComponentCallback load_callback) = 0;

  // Provides an interface to the VideoCaptureService (started lazily by the
  // Chrome service manager) to connect the MediaPerceptionService to it and
  // establish a direct Mojo IPC-based connection.
  // |provider| is owned by the caller.
  virtual void BindDeviceFactoryProviderToVideoCaptureService(
      video_capture::mojom::DeviceFactoryProviderPtr* provider) = 0;

  // Provides an interface to set a handler for an incoming
  // MediaPerceptionRequest.
  virtual void SetMediaPerceptionRequestHandler(
      MediaPerceptionRequestHandler handler) = 0;

  // Receives an incoming media perception request and forwards it to the
  // request handler if set.
  virtual void ForwardMediaPerceptionRequest(
      chromeos::media_perception::mojom::MediaPerceptionRequest request,
      content::RenderFrameHost* render_frame_host) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_API_DELEGATE_H_
