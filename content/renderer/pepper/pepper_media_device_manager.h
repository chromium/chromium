// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_DEVICE_MANAGER_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_DEVICE_MANAGER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "content/renderer/pepper/pepper_device_enumeration_host_helper.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ppapi/c/pp_instance.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace blink {
class WebMediaStreamDeviceObserver;
}  // namespace blink

namespace content {

class PepperMediaDeviceManager
    : public PepperDeviceEnumerationHostHelper::Delegate,
      public blink::mojom::MediaDevicesListener,
      public RenderFrameObserver,
      public RenderFrameObserverTracker<PepperMediaDeviceManager>,
      public base::SupportsWeakPtr<PepperMediaDeviceManager> {
 public:
  static base::WeakPtr<PepperMediaDeviceManager> GetForRenderFrame(
      RenderFrame* render_frame);
  ~PepperMediaDeviceManager() override;

  // PepperDeviceEnumerationHostHelper::Delegate implementation:
  void EnumerateDevices(PP_DeviceType_Dev type,
                        DevicesOnceCallback callback) override;
  size_t StartMonitoringDevices(PP_DeviceType_Dev type,
                                const DevicesCallback& callback) override;
  void StopMonitoringDevices(PP_DeviceType_Dev type,
                             size_t subscription_id) override;

  // blink::mojom::MediaDevicesListener implementation.
  void OnDevicesChanged(
      blink::MediaDeviceType type,
      const blink::WebMediaDeviceInfoArray& device_infos) override;

  using OpenDeviceCallback =
      base::OnceCallback<void(int /* request_id */,
                              bool /* succeeded */,
                              const std::string& /* label */)>;

  // Opens the specified device. The request ID passed into the callback will be
  // the same as the return value. If successful, the label passed into the
  // callback identifies a audio/video steam, which can be used to call
  // CloseDevice() and GetSesssionID().
  int OpenDevice(PP_DeviceType_Dev type,
                 const std::string& device_id,
                 PP_Instance pp_instance,
                 OpenDeviceCallback callback);
  // Cancels an request to open device, using the request ID returned by
  // OpenDevice(). It is guaranteed that the callback passed into OpenDevice()
  // won't be called afterwards.
  void CancelOpenDevice(int request_id);
  void CloseDevice(const std::string& label);
  // Gets audio/video session ID given a label.
  base::UnguessableToken GetSessionID(PP_DeviceType_Dev type,
                                      const std::string& label);

  // Stream type conversion.
  static blink::mojom::MediaStreamType FromPepperDeviceType(
      PP_DeviceType_Dev type);

 private:
  explicit PepperMediaDeviceManager(RenderFrame* render_frame);

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Called by StopEnumerateDevices() after returing to the event loop, to avoid
  // a reentrancy problem.
  void StopEnumerateDevicesDelayed(int request_id);

  void OnDeviceOpened(int request_id,
                      bool success,
                      const std::string& label,
                      const blink::MediaStreamDevice& device);

  void DevicesEnumerated(
      DevicesOnceCallback callback,
      blink::MediaDeviceType type,
      const std::vector<blink::WebMediaDeviceInfoArray>& enumeration,
      std::vector<blink::mojom::VideoInputDeviceCapabilitiesPtr>
          video_input_capabilities,
      std::vector<blink::mojom::AudioInputDeviceCapabilitiesPtr>
          audio_input_capabilities);

  blink::mojom::MediaStreamDispatcherHost* GetMediaStreamDispatcherHost();
  blink::WebMediaStreamDeviceObserver* GetMediaStreamDeviceObserver() const;
  blink::mojom::MediaDevicesDispatcherHost* GetMediaDevicesDispatcher();

  int next_id_ = 1;
  using OpenCallbackMap = std::map<int, OpenDeviceCallback>;
  OpenCallbackMap open_callbacks_;

  using Subscription = std::pair<size_t, DevicesCallback>;
  using SubscriptionList = std::vector<Subscription>;
  SubscriptionList device_change_subscriptions_[blink::NUM_MEDIA_DEVICE_TYPES];

  mojo::Remote<blink::mojom::MediaStreamDispatcherHost> dispatcher_host_;
  mojo::Remote<blink::mojom::MediaDevicesDispatcherHost>
      media_devices_dispatcher_;

  mojo::ReceiverSet<blink::mojom::MediaDevicesListener> receivers_;

  DISALLOW_COPY_AND_ASSIGN(PepperMediaDeviceManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_DEVICE_MANAGER_H_
