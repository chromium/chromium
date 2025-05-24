// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_HAL_DISPATCHER_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_HAL_DISPATCHER_IMPL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/mojo_service_manager_observer.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "media/capture/video/chromeos/token_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {

class SingleThreadTaskRunner;
class WaitableEvent;

}  // namespace base

namespace ash {

class CameraEffectsController;

}  // namespace ash

namespace media {

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
  // |is_new_active_client| is true if the client of |type| becomes active. If
  // it is inactive or already active, |is_new_active_client| is false.
  // |active_device_ids| are device ids of open cameras associated with the
  // client of |type|. If |active_device_ids.empty()|, the client of |type| is
  // inactive. Otherwise, it is active.
  virtual void OnActiveClientChange(
      cros::mojom::CameraClientType type,
      bool is_new_active_client,
      const base::flat_set<std::string>& active_device_ids) {}
};

class CAPTURE_EXPORT CameraPrivacySwitchObserver
    : public base::CheckedObserver {
 public:
  ~CameraPrivacySwitchObserver() override = default;

  virtual void OnCameraHWPrivacySwitchStateChanged(
      const std::string& device_id,
      cros::mojom::CameraPrivacySwitchState state) {}

  virtual void OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState state) {}
};

// CameraEffectObserver is the interface to observe the change of camera
// effects, the observers will be notified with the new effect configurations.
class CAPTURE_EXPORT CameraEffectObserver : public base::CheckedObserver {
 public:
  ~CameraEffectObserver() override = default;

  // Expose the current camera effects to the observers. If the new_effect is
  // null, it indicates that something goes wrong and the set camera effects
  // request is rejected before the mojo call.
  virtual void OnCameraEffectChanged(
      const cros::mojom::EffectsConfigPtr& new_effects) {}
};

// CameraHalDispatcherImpl hosts the CameraHalDispatcher service for Plug-in VM
// host or camera tests to request the camera service.
//
// CameraHalClients connect to the CameraHalDisptcherImpl first, and
// CameraHalDispatcherImpl then creates and dispatches the Mojo channels between
// CameraHalServer and CameraHalClients to establish direct Mojo connections
// between the CameraHalServer and the CameraHalClients.
//
// CameraHalDispatcherImpl owns |proxy_thread_| which is the thread where the
// Mojo channel is bound and all communication through Mojo will happen.
//
// For general documentation about the CameraHalDispatcher Mojo interface see
// the comments in mojo/cros_camera_service.mojom.
class CAPTURE_EXPORT CameraHalDispatcherImpl final
    : public cros::mojom::CameraHalDispatcher,
      public cros::mojom::CrosCameraServiceObserver,
      public chromeos::mojo_service_manager::mojom::ServiceProvider {
 public:
  using CameraEffectsControllerCallback =
      base::RepeatingCallback<void(cros::mojom::EffectsConfigPtr,
                                   cros::mojom::SetEffectResult)>;

  using CameraEffectObserverCallback =
      base::OnceCallback<void(cros::mojom::EffectsConfigPtr)>;

  static CameraHalDispatcherImpl* GetInstance();

  CameraHalDispatcherImpl(const CameraHalDispatcherImpl&) = delete;
  CameraHalDispatcherImpl& operator=(const CameraHalDispatcherImpl&) = delete;

  bool Start();

  void AddClientObserver(CameraClientObserver* observer,
                         base::OnceCallback<void(int32_t)> result_callback);

  bool IsStarted();

  // Adds an observer that watches for active camera client changes. Observer
  // would be immediately notified of the current list of active clients.
  void AddActiveClientObserver(CameraActiveClientObserver* observer);

  // Removes the observer. A previously-added observer must be removed before
  // being destroyed.
  void RemoveActiveClientObserver(CameraActiveClientObserver* observer);

  // Removes the observers after a call by the subject and returns after
  // the observers are removed.
  void RemoveClientObservers(
      std::vector<CameraClientObserver*> client_observers);

  // Adds an observer to get notified when the camera privacy switch state
  // changed. Please note that for some devices, the signal will only be
  // detectable when the camera is currently on due to hardware limitations.
  // Returns the map from device id to the current state of its camera HW
  // privacy switch. Before receiving the first HW privacy switch event for a
  // device, the map has no entry for that device. Otherwise, the map holds the
  // latest reported state for each device.
  base::flat_map<std::string, cros::mojom::CameraPrivacySwitchState>
  AddCameraPrivacySwitchObserver(CameraPrivacySwitchObserver* observer);

  // Removes the observer. A previously-added observer must be removed before
  // being destroyed.
  void RemoveCameraPrivacySwitchObserver(CameraPrivacySwitchObserver* observer);

  // Adds an observer that watches for camera effect configuration change.
  void AddCameraEffectObserver(CameraEffectObserver* observer);

  // Removes the observer. A previously-added observer must be removed before
  // being destroyed.
  void RemoveCameraEffectObserver(CameraEffectObserver* observer);

  // Gets the current camera software privacy switch state.
  void GetCameraSWPrivacySwitchState(
      cros::mojom::CrosCameraService::GetCameraSWPrivacySwitchStateCallback
          callback);

  // Sets the camera software privacy switch state.
  void SetCameraSWPrivacySwitchState(
      cros::mojom::CameraPrivacySwitchState state);

  // Called by vm_permission_service to register the token used for
  // pluginvm.
  void RegisterPluginVmToken(const base::UnguessableToken& token);
  void UnregisterPluginVmToken(const base::UnguessableToken& token);

  // Called by CameraHalDispatcher.
  void AddCameraIdToDeviceIdEntry(int32_t camera_id,
                                  const std::string& device_id);

  // CameraHalDispatcher implementations.
  void RegisterClientWithToken(
      mojo::PendingRemote<cros::mojom::CameraHalClient> client,
      cros::mojom::CameraClientType type,
      const base::UnguessableToken& auth_token,
      RegisterClientWithTokenCallback callback) final;

  // CrosCameraServiceObserver implementations.
  void CameraDeviceActivityChange(int32_t camera_id,
                                  bool opened,
                                  cros::mojom::CameraClientType type) final;
  void CameraPrivacySwitchStateChange(
      cros::mojom::CameraPrivacySwitchState state,
      int32_t camera_id) final;
  void CameraSWPrivacySwitchStateChange(
      cros::mojom::CameraPrivacySwitchState state) final;

  void CameraEffectChange(cros::mojom::EffectsConfigPtr config) final;
  void AutoFramingStateChange(cros::mojom::CameraAutoFramingState state) final;

  base::UnguessableToken GetTokenForTrustedClient(
      cros::mojom::CameraClientType type);

  void SetAutoFramingState(cros::mojom::CameraAutoFramingState state);
  void GetAutoFramingSupported(
      cros::mojom::CrosCameraService::GetAutoFramingSupportedCallback callback);

  // This function needs to be called first before `SetCameraEffects`.
  void SetCameraEffectsControllerCallback(
      CameraEffectsControllerCallback camera_effects__controller_callback);

  // Sets camera effects through `SetCameraEffectsOnProxyThread`.
  // This function should not be called by any client except
  // `CameraEffectsController`. Clients should always use
  // `CameraEffectsController` instead.
  void SetCameraEffects(cros::mojom::EffectsConfigPtr config);

 private:
  friend struct base::DefaultSingletonTraits<CameraHalDispatcherImpl>;
  // Allow the test to construct the class directly.
  friend class CameraHalDispatcherImplTest;
  class VCDInfoObserverImpl;

  CameraHalDispatcherImpl();
  ~CameraHalDispatcherImpl() final;

  bool StartThreads();

  void GetCameraSWPrivacySwitchStateOnProxyThread(
      cros::mojom::CrosCameraService::GetCameraSWPrivacySwitchStateCallback
          callback);

  void SetCameraSWPrivacySwitchStateOnProxyThread(
      cros::mojom::CameraPrivacySwitchState state);

  void AddClientObserverOnProxyThread(
      CameraClientObserver* observer,
      base::OnceCallback<void(int32_t)> result_callback,
      base::WaitableEvent* added);

  void EstablishMojoChannel(CameraClientObserver* client_observer);

  // Handler for incoming Mojo connection requesting CameraHalDispatcher
  // service.
  void OnPeerConnected(mojo::ScopedMessagePipeHandle message_pipe);

  // Mojo connection error handlers.
  void OnCameraServiceConnectionError();
  void OnCameraHalClientConnectionError(CameraClientObserver* client);

  void OnGetCameraModule(
      CameraClientObserver* client_observer,
      mojo::PendingRemote<cros::mojom::CameraModule> camera_module);

  // Cleans up everything about the observer
  void CleanupClientOnProxyThread(CameraClientObserver* client_observer);
  void RemoveClientObserversOnProxyThread(
      std::vector<CameraClientObserver*> client_observers,
      base::WaitableEvent* removed);

  void RegisterClientWithTokenOnProxyThread(
      mojo::PendingRemote<cros::mojom::CameraHalClient> client,
      cros::mojom::CameraClientType type,
      base::UnguessableToken token,
      RegisterClientWithTokenCallback callback);

  void SetAutoFramingStateOnProxyThread(
      cros::mojom::CameraAutoFramingState state);
  void GetAutoFramingSupportedOnProxyThread(
      cros::mojom::CrosCameraService::GetAutoFramingSupportedCallback callback);

  // Calls the `camera_hal_server_` to set the camera effects.
  void SetCameraEffectsOnProxyThread(cros::mojom::EffectsConfigPtr config,
                                     bool is_from_register);

  // Calls the `camera_hal_server_` to set the initial camera effects.
  void SetInitialCameraEffectsOnProxyThread(
      cros::mojom::EffectsConfigPtr config);

  // Called when camera_hal_server_->SetCameraEffect returns.
  void OnSetCameraEffectsCompleteOnProxyThread(
      cros::mojom::EffectsConfigPtr config,
      bool is_from_register,
      cros::mojom::SetEffectResult result);

  void BindCameraServiceOnProxyThread(
      mojo::PendingRemote<cros::mojom::CrosCameraService> camera_service);

  void TryConnectToCameraService();

  std::string GetDeviceIdFromCameraId(int32_t camera_id);
  base::flat_set<std::string> GetDeviceIdsFromCameraIds(
      const base::flat_set<int32_t>& camera_ids);

  void StopOnProxyThread();

  TokenManager* GetTokenManagerForTesting();

  // chromeos::mojo_service_manager::mojom::ServiceProvider overrides.
  void Request(
      chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
      mojo::ScopedMessagePipeHandle receiver) override;

  base::Thread proxy_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> proxy_task_runner_;

  mojo::ReceiverSet<cros::mojom::CameraHalDispatcher> receiver_set_;

  mojo::Remote<cros::mojom::CrosCameraService> camera_service_;

  mojo::Receiver<cros::mojom::CrosCameraServiceObserver>
      camera_service_observer_receiver_;

  std::set<raw_ptr<CameraClientObserver, SetExperimental>> client_observers_;

  TokenManager token_manager_;

  base::Lock opened_camera_id_map_lock_;
  base::flat_map<cros::mojom::CameraClientType, base::flat_set<int32_t>>
      opened_camera_id_map_ GUARDED_BY(opened_camera_id_map_lock_);

  scoped_refptr<base::ObserverListThreadSafe<CameraActiveClientObserver>>
      active_client_observers_;

  // |device_id_to_hw_privacy_switch_state_| can be accessed from the UI thread
  // besides |proxy_thread_|.
  base::Lock device_id_to_hw_privacy_switch_state_lock_;
  base::flat_map<std::string, cros::mojom::CameraPrivacySwitchState>
      device_id_to_hw_privacy_switch_state_
          GUARDED_BY(device_id_to_hw_privacy_switch_state_lock_);

  cros::mojom::CameraAutoFramingState current_auto_framing_state_ =
      cros::mojom::CameraAutoFramingState::OFF;

  cros::mojom::CrosCameraService::GetAutoFramingSupportedCallback
      auto_framing_supported_callback_;

  // Records current successfully set camera effects.
  // Used inside RegisterServerWithToken.
  cros::mojom::EffectsConfigPtr current_effects_;
  // The initial state the camera effects should be set to
  // when the camera server is registered.
  cros::mojom::EffectsConfigPtr initial_effects_;

  scoped_refptr<base::ObserverListThreadSafe<CameraPrivacySwitchObserver>>
      privacy_switch_observers_;

  scoped_refptr<base::ObserverListThreadSafe<CameraEffectObserver>>
      camera_effect_observers_;

  std::map<CameraClientObserver*, std::unique_ptr<CameraClientObserver>>
      mojo_client_observers_;

  // A map from camera id to |VideoCaptureDeviceDescriptor.device_id|, which is
  // updated in CameraHalDelegate::GetDevicesInfo() and queried in
  // GetDeviceIdFromCameraId().
  base::Lock camera_id_to_device_id_lock_;
  base::flat_map<int32_t, std::string> camera_id_to_device_id_
      GUARDED_BY(camera_id_to_device_id_lock_);

  // Receiver for mojo service manager service provider.
  mojo::Receiver<chromeos::mojo_service_manager::mojom::ServiceProvider>
      provider_receiver_{this};

  std::unique_ptr<MojoServiceManagerObserver> mojo_service_manager_observer_;

  std::unique_ptr<VCDInfoObserverImpl> vcd_info_observer_impl_;

  base::WeakPtrFactory<CameraHalDispatcherImpl> weak_factory_{this};
};

}  // namespace media

namespace base {

template <>
struct ScopedObservationTraits<media::CameraHalDispatcherImpl,
                               media::CameraEffectObserver> {
  static void AddObserver(media::CameraHalDispatcherImpl* source,
                          media::CameraEffectObserver* observer) {
    source->AddCameraEffectObserver(observer);
  }
  static void RemoveObserver(media::CameraHalDispatcherImpl* source,
                             media::CameraEffectObserver* observer) {
    source->RemoveCameraEffectObserver(observer);
  }
};

}  // namespace base

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_HAL_DISPATCHER_IMPL_H_
