// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_ASH_CAMERA_HAL_DISPATCHER_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_ASH_CAMERA_HAL_DISPATCHER_IMPL_H_

#include <memory>
#include <set>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "base/observer_list_types.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "base/unguessable_token.h"
#include "components/chromeos_camera/common/jpeg_encode_accelerator.mojom.h"
#include "components/chromeos_camera/common/mjpeg_decode_accelerator.mojom.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "media/capture/video/chromeos/token_manager.h"
#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"

namespace base {

class SingleThreadTaskRunner;
class WaitableEvent;

}  // namespace base

namespace media {

using MojoJpegEncodeAcceleratorFactoryCB = base::RepeatingCallback<void(
    mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>)>;

class CAPTURE_EXPORT CameraClientObserver {
 public:
  CameraClientObserver(cros::mojom::CameraClientType type,
                       base::UnguessableToken auth_token)
      : type_(type), auth_token_(auth_token) {}
  virtual ~CameraClientObserver();
  virtual void OnChannelCreated(
      mojo::PendingRemote<cros::mojom::CameraModule> camera_module) = 0;

  cros::mojom::CameraClientType GetType() { return type_; }
  const base::UnguessableToken GetAuthToken() { return auth_token_; }

  bool Authenticate(TokenManager* token_manager);

 private:
  cros::mojom::CameraClientType type_;
  base::UnguessableToken auth_token_;
};

class CAPTURE_EXPORT CameraActiveClientObserver : public base::CheckedObserver {
 public:
  virtual void OnActiveClientChange(cros::mojom::CameraClientType type,
                                    bool is_active) = 0;
};

// A class to provide a no-op remote to CameraHalServer that failed
// registration. When CameraHalServer calls
// CameraHalDispatcher::RegisterServerWithToken to register itself, a
// PendingRemote<CameraHalServerCallbacks> is returned. Returning an unbound
// pending remote would crash CameraHalServer immediately, and thus disallows
// it from handling authentication failures.
// TODO(b/170075468): Modify RegisterServerWithToken to return an optional
// CameraHalServerCallbacks instead.
class FailedCameraHalServerCallbacks
    : public cros::mojom::CameraHalServerCallbacks {
 private:
  friend class CameraHalDispatcherImpl;

  FailedCameraHalServerCallbacks();
  ~FailedCameraHalServerCallbacks() final;

  mojo::PendingRemote<cros::mojom::CameraHalServerCallbacks> GetRemote();

  // CameraHalServerCallbacks implementations.
  void CameraDeviceActivityChange(int32_t camera_id,
                                  bool opened,
                                  cros::mojom::CameraClientType type) final;
  void CameraPrivacySwitchStateChange(
      cros::mojom::CameraPrivacySwitchState state) final;

  mojo::Receiver<cros::mojom::CameraHalServerCallbacks> callbacks_;
};

class CAPTURE_EXPORT CameraPrivacySwitchObserver
    : public base::CheckedObserver {
 public:
  virtual void OnCameraPrivacySwitchStatusChanged(
      cros::mojom::CameraPrivacySwitchState state) = 0;

 protected:
  ~CameraPrivacySwitchObserver() override = default;
};

// The CameraHalDispatcherImpl hosts and waits on the unix domain socket
// /var/run/camera3.sock.  CameraHalServer and CameraHalClients connect to the
// unix domain socket to create the initial Mojo connections with the
// CameraHalDisptcherImpl, and CameraHalDispatcherImpl then creates and
// dispaches the Mojo channels between CameraHalServer and CameraHalClients to
// establish direct Mojo connections between the CameraHalServer and the
// CameraHalClients.
//
// For general documentation about the CameraHalDispater Mojo interface see the
// comments in mojo/cros_camera_service.mojom.
class CAPTURE_EXPORT CameraHalDispatcherImpl final
    : public cros::mojom::CameraHalDispatcher,
      public cros::mojom::CameraHalServerCallbacks,
      public base::trace_event::TraceLog::EnabledStateObserver {
 public:
  static CameraHalDispatcherImpl* GetInstance();

  bool Start(MojoMjpegDecodeAcceleratorFactoryCB jda_factory,
             MojoJpegEncodeAcceleratorFactoryCB jea_factory);

  void AddClientObserver(std::unique_ptr<CameraClientObserver> observer,
                         base::OnceCallback<void(int32_t)> result_callback);

  bool IsStarted();

  // Adds an observer that watches for active camera client changes. Observer
  // would be immediately notified of the current list of active clients.
  void AddActiveClientObserver(CameraActiveClientObserver* observer);

  // Removes the observer. A previously-added observer must be removed before
  // being destroyed.
  void RemoveActiveClientObserver(CameraActiveClientObserver* observer);

  // Adds an observer to get notified when the camera privacy switch status
  // changed. Please note that for some devices, the signal will only be
  // detectable when the camera is currently on due to hardware limitations.
  // Returns the current state of the camera privacy switch.
  cros::mojom::CameraPrivacySwitchState AddCameraPrivacySwitchObserver(
      CameraPrivacySwitchObserver* observer);

  // Removes the observer. A previously-added observer must be removed before
  // being destroyed.
  void RemoveCameraPrivacySwitchObserver(CameraPrivacySwitchObserver* observer);

  // Called by vm_permission_service to register the token used for pluginvm.
  void RegisterPluginVmToken(const base::UnguessableToken& token);
  void UnregisterPluginVmToken(const base::UnguessableToken& token);

  // CameraHalDispatcher implementations.
  void RegisterServer(
      mojo::PendingRemote<cros::mojom::CameraHalServer> server) final;
  void RegisterServerWithToken(
      mojo::PendingRemote<cros::mojom::CameraHalServer> server,
      const base::UnguessableToken& token,
      RegisterServerWithTokenCallback callback) final;
  void RegisterClient(
      mojo::PendingRemote<cros::mojom::CameraHalClient> client) final;
  void RegisterClientWithToken(
      mojo::PendingRemote<cros::mojom::CameraHalClient> client,
      cros::mojom::CameraClientType type,
      const base::UnguessableToken& auth_token,
      RegisterClientWithTokenCallback callback) final;
  void GetJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          jda_receiver) final;
  void GetJpegEncodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
          jea_receiver) final;

  // CameraHalServerCallbacks implementations.
  void CameraDeviceActivityChange(int32_t camera_id,
                                  bool opened,
                                  cros::mojom::CameraClientType type) final;
  void CameraPrivacySwitchStateChange(
      cros::mojom::CameraPrivacySwitchState state) final;

  base::UnguessableToken GetTokenForTrustedClient(
      cros::mojom::CameraClientType type);

  // base::trace_event::TraceLog::EnabledStateObserver implementation.
  void OnTraceLogEnabled() final;
  void OnTraceLogDisabled() final;

 private:
  friend struct base::DefaultSingletonTraits<CameraHalDispatcherImpl>;
  // Allow the test to construct the class directly.
  friend class CameraHalDispatcherImplTest;

  CameraHalDispatcherImpl();
  ~CameraHalDispatcherImpl() final;

  bool StartThreads();

  // Creates the unix domain socket for the camera client processes and the
  // camera HALv3 adapter process to connect.
  void CreateSocket(base::WaitableEvent* started);

  // Waits for incoming connections (from HAL process or from client processes).
  // Runs on |blocking_io_thread_|.
  void StartServiceLoop(base::ScopedFD socket_fd, base::WaitableEvent* started);

  void RegisterClientWithTokenOnProxyThread(
      mojo::PendingRemote<cros::mojom::CameraHalClient> client,
      cros::mojom::CameraClientType type,
      base::UnguessableToken token,
      RegisterClientWithTokenCallback callback);

  void AddClientObserverOnProxyThread(
      std::unique_ptr<CameraClientObserver> observer,
      base::OnceCallback<void(int32_t)> result_callback);

  void EstablishMojoChannel(CameraClientObserver* client_observer);

  // Handler for incoming Mojo connection on the unix domain socket.
  void OnPeerConnected(mojo::ScopedMessagePipeHandle message_pipe);

  // Mojo connection error handlers.
  void OnCameraHalServerConnectionError();
  void OnCameraHalClientConnectionError(CameraClientObserver* client);

  void StopOnProxyThread();

  void OnTraceLogEnabledOnProxyThread();
  void OnTraceLogDisabledOnProxyThread();

  TokenManager* GetTokenManagerForTesting();

  base::ScopedFD proxy_fd_;
  base::ScopedFD cancel_pipe_;

  base::Thread proxy_thread_;
  base::Thread blocking_io_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> proxy_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> blocking_io_task_runner_;

  mojo::ReceiverSet<cros::mojom::CameraHalDispatcher> receiver_set_;

  mojo::Remote<cros::mojom::CameraHalServer> camera_hal_server_;

  mojo::Receiver<cros::mojom::CameraHalServerCallbacks>
      camera_hal_server_callbacks_;
  FailedCameraHalServerCallbacks failed_camera_hal_server_callbacks_;

  std::set<std::unique_ptr<CameraClientObserver>, base::UniquePtrComparator>
      client_observers_;

  MojoMjpegDecodeAcceleratorFactoryCB jda_factory_;

  MojoJpegEncodeAcceleratorFactoryCB jea_factory_;

  TokenManager token_manager_;

  base::Lock opened_camera_id_map_lock_;
  base::flat_map<cros::mojom::CameraClientType, base::flat_set<int32_t>>
      opened_camera_id_map_ GUARDED_BY(opened_camera_id_map_lock_);

  scoped_refptr<base::ObserverListThreadSafe<CameraActiveClientObserver>>
      active_client_observers_;

  base::Lock privacy_switch_state_lock_;
  cros::mojom::CameraPrivacySwitchState current_privacy_switch_state_
      GUARDED_BY(privacy_switch_state_lock_);

  scoped_refptr<base::ObserverListThreadSafe<CameraPrivacySwitchObserver>>
      privacy_switch_observers_;

  DISALLOW_COPY_AND_ASSIGN(CameraHalDispatcherImpl);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_ASH_CAMERA_HAL_DISPATCHER_IMPL_H_
