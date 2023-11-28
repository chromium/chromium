// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <sys/uio.h>

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "components/device_event_log/device_event_log.h"
#include "media/capture/video/chromeos/mojom/camera_common.mojom.h"
#include "media/capture/video/chromeos/mojom/cros_camera_client.mojom.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "media/capture/video/chromeos/mojom/effects_pipeline.mojom.h"
#include "media/capture/video/chromeos/video_capture_features_chromeos.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"
#include "mojo/public/cpp/system/invitation.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

using chromeos::mojo_service_manager::mojom::ErrorOrServiceState;
using chromeos::mojo_service_manager::mojom::ServiceState;

namespace media {

namespace {

const base::FilePath::CharType kArcCamera3SocketPath[] =
    "/run/camera/camera3.sock";
const char kArcCameraGroup[] = "arc-camera";
const base::FilePath::CharType kForceEnableAePath[] =
    "/run/camera/force_enable_face_ae";
const base::FilePath::CharType kForceDisableAePath[] =
    "/run/camera/force_disable_face_ae";
const base::FilePath::CharType kForceEnableAutoFramingPath[] =
    "/run/camera/force_enable_auto_framing";
const base::FilePath::CharType kForceDisableAutoFramingPath[] =
    "/run/camera/force_disable_auto_framing";
const base::FilePath::CharType kForceEnableEffectsPath[] =
    "/run/camera/force_enable_effects";
const base::FilePath::CharType kForceDisableEffectsPath[] =
    "/run/camera/force_disable_effects";

void CreateEnableDisableFile(const std::string& enable_path,
                             const std::string& disable_path,
                             bool should_enable,
                             bool should_remove_both) {
  base::FilePath enable_file_path(enable_path);
  base::FilePath disable_file_path(disable_path);

  // Removing enable file if the target is to disable or remove both.
  if ((!should_enable || should_remove_both) &&
      !base::DeleteFile(enable_file_path)) {
    LOG(WARNING) << "CameraHalDispatcherImpl Error: can't  delete "
                 << enable_file_path;
  }

  // Removing disable file if the target is to enable or remove both.
  if ((should_enable || should_remove_both) &&
      !base::DeleteFile(disable_file_path)) {
    LOG(WARNING) << "CameraHalDispatcherImpl Error: can't delete "
                 << disable_file_path;
  }

  if (should_remove_both) {
    return;
  }

  const base::FilePath& new_file =
      should_enable ? enable_file_path : disable_file_path;

  // Adding enable/disable file if it does not exist yet.
  if (!base::PathExists(new_file)) {
    base::File file(new_file,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    file.Close();
  }
}

std::string GenerateRandomToken() {
  char random_bytes[16];
  base::RandBytes(random_bytes, 16);
  return base::HexEncode(random_bytes, 16);
}

// Waits until |raw_socket_fd| is readable.  We signal |raw_cancel_fd| when we
// want to cancel the blocking wait and stop serving connections on
// |raw_socket_fd|.  To notify such a situation, |raw_cancel_fd| is also passed
// to here, and the write side will be closed in such a case.
bool WaitForSocketReadable(int raw_socket_fd, int raw_cancel_fd) {
  struct pollfd fds[2] = {
      {raw_socket_fd, POLLIN, 0},
      {raw_cancel_fd, POLLIN, 0},
  };

  if (HANDLE_EINTR(poll(fds, std::size(fds), -1)) <= 0) {
    PLOG(ERROR) << "poll()";
    return false;
  }

  if (fds[1].revents) {
    VLOG(1) << "Stop() was called";
    return false;
  }

  DCHECK(fds[0].revents);
  return true;
}

bool HasCrosCameraTest() {
  static constexpr char kCrosCameraTestPath[] =
      "/usr/local/bin/cros_camera_test";

  base::FilePath path(kCrosCameraTestPath);
  return base::PathExists(path);
}

class MojoCameraClientObserver : public CameraClientObserver {
 public:
  MojoCameraClientObserver() = delete;

  explicit MojoCameraClientObserver(
      mojo::PendingRemote<cros::mojom::CameraHalClient> client,
      cros::mojom::CameraClientType type,
      base::UnguessableToken auth_token)
      : CameraClientObserver(type, std::move(auth_token)),
        client_(std::move(client)) {}

  MojoCameraClientObserver(const MojoCameraClientObserver&) = delete;
  MojoCameraClientObserver& operator=(const MojoCameraClientObserver&) = delete;

  void OnChannelCreated(
      mojo::PendingRemote<cros::mojom::CameraModule> camera_module) override {
    client_->SetUpChannel(std::move(camera_module));
  }

  mojo::Remote<cros::mojom::CameraHalClient>& client() { return client_; }

 private:
  mojo::Remote<cros::mojom::CameraHalClient> client_;
};

}  // namespace

CameraClientObserver::~CameraClientObserver() = default;

bool CameraClientObserver::Authenticate(TokenManager* token_manager) {
  auto authenticated_type =
      token_manager->AuthenticateClient(type_, auth_token_);
  if (!authenticated_type) {
    return false;
  }
  type_ = authenticated_type.value();
  return true;
}

// static
CameraHalDispatcherImpl* CameraHalDispatcherImpl::GetInstance() {
  return base::Singleton<CameraHalDispatcherImpl>::get();
}

bool CameraHalDispatcherImpl::StartThreads() {
  DCHECK(!proxy_thread_.IsRunning());
  DCHECK(!blocking_io_thread_.IsRunning());

  if (!proxy_thread_.Start()) {
    LOG(ERROR) << "Failed to start proxy thread";
    return false;
  }
  if (!blocking_io_thread_.Start()) {
    LOG(ERROR) << "Failed to start blocking IO thread";
    proxy_thread_.Stop();
    return false;
  }
  proxy_task_runner_ = proxy_thread_.task_runner();
  blocking_io_task_runner_ = blocking_io_thread_.task_runner();
  return true;
}

void CameraHalDispatcherImpl::BindCameraServiceOnProxyThread(
    mojo::PendingRemote<cros::mojom::CrosCameraService> camera_service) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  DCHECK(!camera_service_.is_bound());

  CAMERA_LOG(EVENT) << "Connected to cros-camera.";
  camera_service_.Bind(std::move(camera_service));
  camera_service_.set_disconnect_handler(
      base::BindOnce(&CameraHalDispatcherImpl::OnCameraServiceConnectionError,
                     base::Unretained(this)));
  if (auto_framing_supported_callback_) {
    camera_service_->GetAutoFramingSupported(
        std::move(auto_framing_supported_callback_));
  }
  camera_service_->SetAutoFramingState(current_auto_framing_state_);

  // Should only be called when an effect is set.
  if (!initial_effects_.is_null() || !current_effects_.is_null()) {
    // If current_effects_ is set, then a newer effect was applied since
    // the initial setup and we should use that, as the camera server
    // may have crashed and restarted.
    cros::mojom::EffectsConfigPtr& config =
        current_effects_.is_null() ? initial_effects_ : current_effects_;

    SetCameraEffectsOnProxyThread(config.Clone(), /*is_from_register=*/true);
  }
  camera_service_->AddCrosCameraServiceObserver(
      camera_service_observer_receiver_.BindNewPipeAndPassRemote());
  // Set up the Mojo channels for clients which registered before cros camera
  // service starts or that have disconnected from the camera module because the
  // cros camera service stopped.
  for (auto* client_observer : client_observers_) {
    EstablishMojoChannel(client_observer);
  }
}

void CameraHalDispatcherImpl::TryConnectToCameraService() {
  CHECK(ash::mojo_service_manager::IsServiceManagerBound());

  mojo::PendingRemote<cros::mojom::CrosCameraService> camera_service;
  ash::mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosCameraService, absl::nullopt,
      camera_service.InitWithNewPipeAndPassReceiver().PassPipe());
  proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImpl::BindCameraServiceOnProxyThread,
                     base::Unretained(this), std::move(camera_service)));
}

bool CameraHalDispatcherImpl::Start() {
  DCHECK(!IsStarted());
  if (!StartThreads()) {
    return false;
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  CreateEnableDisableFile(
      kForceEnableAePath, kForceDisableAePath,
      /*should_enable=*/
      command_line->GetSwitchValueASCII(media::switches::kForceControlFaceAe) ==
          "enable",
      /*should_remove_both=*/
      !command_line->HasSwitch(media::switches::kForceControlFaceAe));

  CreateEnableDisableFile(
      kForceEnableAutoFramingPath, kForceDisableAutoFramingPath,
      /*should_enable=*/
      command_line->GetSwitchValueASCII(switches::kAutoFramingOverride) ==
          switches::kAutoFramingForceEnabled,
      /*should_remove_both=*/
      !command_line->HasSwitch(media::switches::kAutoFramingOverride));

  CreateEnableDisableFile(
      kForceEnableEffectsPath, kForceDisableEffectsPath,
      /*should_enable=*/ash::features::IsVideoConferenceEnabled(),
      /*should_remove_both=*/false);

  base::WaitableEvent started;
  // It's important we generate tokens before creating the socket, because
  // once it is available, everyone connecting to socket would start fetching
  // tokens.
  if (HasCrosCameraTest() && !token_manager_.GenerateTestClientToken()) {
    LOG(ERROR) << "Failed to generate token for test client";
    return false;
  }

  // TODO(b/228238413): In VCD unittests, the endpoint of mojo service
  // manager is not bound. Bind it and request the CrosCameraService service
  // from it to enable real cameras.
  mojo_service_manager_observer_ = MojoServiceManagerObserver::Create(
      chromeos::mojo_services::kCrosCameraService,
      base::BindRepeating(&CameraHalDispatcherImpl::TryConnectToCameraService,
                          weak_factory_.GetWeakPtr()),
      base::DoNothing());
  if (ash::mojo_service_manager::IsServiceManagerBound()) {
    auto* proxy = ash::mojo_service_manager::GetServiceManagerProxy();
    proxy->Register(
        /*service_name=*/chromeos::mojo_services::kCrosCameraHalDispatcher,
        provider_receiver_.BindNewPipeAndPassRemote());
  }

  blocking_io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImpl::CreateSocket,
                     base::Unretained(this), base::Unretained(&started)));
  started.Wait();
  return IsStarted();
}

void CameraHalDispatcherImpl::AddClientObserver(
    CameraClientObserver* observer,
    base::OnceCallback<void(int32_t)> result_callback) {
  // If |proxy_thread_| fails to start in Start() then CameraHalDelegate will
  // not be created, and this function will not be called.
  DCHECK(proxy_thread_.IsRunning());
  base::WaitableEvent added;
  proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImpl::AddClientObserverOnProxyThread,
                     base::Unretained(this), observer,
                     std::move(result_callback), base::Unretained(&added)));
  added.Wait();
}

bool CameraHalDispatcherImpl::IsStarted() {
  return proxy_thread_.IsRunning() && blocking_io_thread_.IsRunning() &&
         proxy_fd_.is_valid();
}

void CameraHalDispatcherImpl::AddActiveClientObserver(
    CameraActiveClientObserver* observer) {
  base::AutoLock lock(opened_camera_id_map_lock_);
  for (auto& [camera_client_type, camera_id_set] : opened_camera_id_map_) {
    if (!camera_id_set.empty()) {
      observer->OnActiveClientChange(camera_client_type,
                                     /*is_new_active_client=*/true,
                                     GetDeviceIdsFromCameraIds(camera_id_set));
    }
  }
  active_client_observers_->AddObserver(observer);
}

void CameraHalDispatcherImpl::RemoveActiveClientObserver(
    CameraActiveClientObserver* observer) {
  active_client_observers_->RemoveObserver(observer);
}

base::flat_map<std::string, cros::mojom::CameraPrivacySwitchState>
CameraHalDispatcherImpl::AddCameraPrivacySwitchObserver(
    CameraPrivacySwitchObserver* observer) {
  privacy_switch_observers_->AddObserver(observer);
  base::AutoLock lock(device_id_to_hw_privacy_switch_state_lock_);
  return device_id_to_hw_privacy_switch_state_;
}

void CameraHalDispatcherImpl::RemoveCameraPrivacySwitchObserver(
    CameraPrivacySwitchObserver* observer) {
  privacy_switch_observers_->RemoveObserver(observer);
}

void CameraHalDispatcherImpl::AddCameraEffectObserver(
    CameraEffectObserver* observer,
    CameraEffectObserverCallback camera_effect_observer_callback) {
  camera_effect_observers_->AddObserver(observer);

  if (proxy_thread_.IsRunning() && !camera_effect_observer_callback.is_null()) {
    proxy_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CameraHalDispatcherImpl::OnCameraEffectsObserverAddOnProxyThread,
            base::Unretained(this),
            std::move(camera_effect_observer_callback)));
  }
}

void CameraHalDispatcherImpl::RemoveCameraEffectObserver(
    CameraEffectObserver* observer) {
  camera_effect_observers_->RemoveObserver(observer);
}

void CameraHalDispatcherImpl::GetCameraSWPrivacySwitchState(
    cros::mojom::CrosCameraService::GetCameraSWPrivacySwitchStateCallback
        callback) {
  if (!proxy_thread_.IsRunning()) {
    LOG(ERROR) << "CameraProxyThread is not started. Failed to query the "
                  "camera SW privacy switch state";
    std::move(callback).Run(cros::mojom::CameraPrivacySwitchState::UNKNOWN);
    return;
  }
  // Unretained reference is safe here because CameraHalDispatcherImpl owns
  // |proxy_thread_|.
  proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImpl::GetCameraSWPrivacySwitchStateOnProxyThread,
          base::Unretained(this), std::move(callback)));
}

void CameraHalDispatcherImpl::SetCameraSWPrivacySwitchState(
    cros::mojom::CameraPrivacySwitchState state) {
  if (!proxy_thread_.IsRunning()) {
    LOG(ERROR) << "CameraProxyThread is not started. "
                  "SetCameraSWPrivacySwitchState request was aborted";
    return;
  }
  // Unretained reference is safe here because CameraHalDispatcherImpl owns
  // |proxy_thread_|.
  proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImpl::SetCameraSWPrivacySwitchStateOnProxyThread,
          base::Unretained(this), state));
}

void CameraHalDispatcherImpl::RegisterPluginVmToken(
    const base::UnguessableToken& token) {
  token_manager_.RegisterPluginVmToken(token);
}

void CameraHalDispatcherImpl::UnregisterPluginVmToken(
    const base::UnguessableToken& token) {
  token_manager_.UnregisterPluginVmToken(token);
}

void CameraHalDispatcherImpl::AddCameraIdToDeviceIdEntry(
    int32_t camera_id,
    const std::string& device_id) {
  base::AutoLock lock(camera_id_to_device_id_lock_);
  camera_id_to_device_id_[camera_id] = device_id;
}

CameraHalDispatcherImpl::CameraHalDispatcherImpl()
    : is_service_loop_running_(false),
      proxy_thread_("CameraProxyThread"),
      blocking_io_thread_("CameraBlockingIOThread"),
      camera_service_observer_receiver_(this),
      active_client_observers_(
          new base::ObserverListThreadSafe<CameraActiveClientObserver>()),
      privacy_switch_observers_(
          new base::ObserverListThreadSafe<CameraPrivacySwitchObserver>()),
      camera_effect_observers_(
          new base::ObserverListThreadSafe<CameraEffectObserver>()) {}

CameraHalDispatcherImpl::~CameraHalDispatcherImpl() {
  VLOG(1) << "Stopping CameraHalDispatcherImpl...";
  if (proxy_thread_.IsRunning()) {
    proxy_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CameraHalDispatcherImpl::StopOnProxyThread,
                                  base::Unretained(this)));
    proxy_thread_.Stop();
  }
  blocking_io_thread_.Stop();
  CAMERA_LOG(EVENT) << "CameraHalDispatcherImpl stopped";
}

void CameraHalDispatcherImpl::RegisterClientWithToken(
    mojo::PendingRemote<cros::mojom::CameraHalClient> client,
    cros::mojom::CameraClientType type,
    const base::UnguessableToken& auth_token,
    RegisterClientWithTokenCallback callback) {
  base::UnguessableToken client_auth_token = auth_token;
  // Unretained reference is safe here because CameraHalDispatcherImpl owns
  // |proxy_thread_|.
  proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImpl::RegisterClientWithTokenOnProxyThread,
          base::Unretained(this), std::move(client), type,
          std::move(client_auth_token),
          base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void CameraHalDispatcherImpl::CameraDeviceActivityChange(
    int32_t camera_id,
    bool opened,
    cros::mojom::CameraClientType type) {
  VLOG(1) << type << (opened ? " opened " : " closed ") << "camera "
          << camera_id;
  base::AutoLock lock(opened_camera_id_map_lock_);
  auto& camera_id_set = opened_camera_id_map_[type];
  if (opened) {
    auto result = camera_id_set.insert(camera_id);
    if (!result.second) {  // No element inserted.
      LOG(WARNING) << "Received duplicated open notification for camera "
                   << camera_id;
      return;
    }
    if (camera_id_set.size() == 1) {
      VLOG(1) << type << " is active";
    }
  } else {
    auto it = camera_id_set.find(camera_id);
    if (it == camera_id_set.end()) {
      // This can happen if something happened to the client process and it
      // simultaneous lost connections to both CameraHalDispatcher and
      // CameraHalServer.
      LOG(WARNING) << "Received close notification for camera " << camera_id
                   << " which is not opened";
      return;
    }
    camera_id_set.erase(it);
    if (camera_id_set.empty()) {
      VLOG(1) << type << " is inactive";
    }
  }
  bool is_new_active_client = camera_id_set.size() == 1 && opened;
  active_client_observers_->Notify(
      FROM_HERE, &CameraActiveClientObserver::OnActiveClientChange, type,
      is_new_active_client, GetDeviceIdsFromCameraIds(camera_id_set));
}

void CameraHalDispatcherImpl::CameraPrivacySwitchStateChange(
    cros::mojom::CameraPrivacySwitchState state,
    int32_t camera_id) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  const std::string& device_id = GetDeviceIdFromCameraId(camera_id);
  base::AutoLock lock(device_id_to_hw_privacy_switch_state_lock_);
  device_id_to_hw_privacy_switch_state_[device_id] = state;
  privacy_switch_observers_->Notify(
      FROM_HERE,
      &CameraPrivacySwitchObserver::OnCameraHWPrivacySwitchStateChanged,
      device_id, state);
  CAMERA_LOG(EVENT) << "Camera privacy switch state changed: " << state;
}

void CameraHalDispatcherImpl::CameraSWPrivacySwitchStateChange(
    cros::mojom::CameraPrivacySwitchState state) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());

  privacy_switch_observers_->Notify(
      FROM_HERE,
      &CameraPrivacySwitchObserver::OnCameraSWPrivacySwitchStateChanged, state);
  CAMERA_LOG(EVENT) << "Camera software privacy switch state changed: "
                    << state;
}

base::UnguessableToken CameraHalDispatcherImpl::GetTokenForTrustedClient(
    cros::mojom::CameraClientType type) {
  return token_manager_.GetTokenForTrustedClient(type);
}

void CameraHalDispatcherImpl::CreateSocket(base::WaitableEvent* started) {
  DCHECK(blocking_io_task_runner_->BelongsToCurrentThread());

  base::FilePath socket_path(kArcCamera3SocketPath);
  mojo::NamedPlatformChannel::Options options;
  options.server_name = socket_path.value();
  mojo::NamedPlatformChannel channel(options);
  if (!channel.server_endpoint().is_valid()) {
    LOG(ERROR) << "Failed to create the socket file: " << kArcCamera3SocketPath;
    started->Signal();
    return;
  }

  // TODO(crbug.com/1053569): Remove these lines once the issue is solved.
  base::File::Info info;
  if (!base::GetFileInfo(socket_path, &info)) {
    LOG(WARNING) << "Failed to get the socket info after building Mojo channel";
  } else {
    LOG(WARNING) << "Building Mojo channel. Socket info:"
                 << " creation_time: " << info.creation_time
                 << " last_accessed: " << info.last_accessed
                 << " last_modified: " << info.last_modified;
  }

  // Change permissions on the socket.
  struct group arc_camera_group;
  struct group* result = nullptr;
  char buf[1024];
  if (HANDLE_EINTR(getgrnam_r(kArcCameraGroup, &arc_camera_group, buf,
                              sizeof(buf), &result)) < 0) {
    PLOG(ERROR) << "getgrnam_r()";
    started->Signal();
    return;
  }

  if (!result) {
    LOG(ERROR) << "Group '" << kArcCameraGroup << "' not found";
    started->Signal();
    return;
  }

  if (HANDLE_EINTR(chown(kArcCamera3SocketPath, -1, arc_camera_group.gr_gid)) <
      0) {
    PLOG(ERROR) << "chown()";
    started->Signal();
    return;
  }

  if (!base::SetPosixFilePermissions(socket_path, 0660)) {
    PLOG(ERROR) << "Could not set permissions: " << socket_path.value();
    started->Signal();
    return;
  }

  blocking_io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImpl::StartServiceLoop,
                     base::Unretained(this),
                     channel.TakeServerEndpoint().TakePlatformHandle().TakeFD(),
                     base::Unretained(started)));
}

bool CameraHalDispatcherImpl::IsServiceLoopRunning() {
  base::AutoLock lock(service_loop_status_lock_);
  return is_service_loop_running_;
}

void CameraHalDispatcherImpl::SetServiceLoopStatus(bool is_running) {
  base::AutoLock lock(service_loop_status_lock_);
  is_service_loop_running_ = is_running;
}

void CameraHalDispatcherImpl::StartServiceLoop(base::ScopedFD socket_fd,
                                               base::WaitableEvent* started) {
  DCHECK(blocking_io_task_runner_->BelongsToCurrentThread());
  DCHECK(!proxy_fd_.is_valid());
  DCHECK(!cancel_pipe_.is_valid());
  DCHECK(socket_fd.is_valid());

  base::ScopedFD cancel_fd;
  if (!base::CreatePipe(&cancel_fd, &cancel_pipe_, true)) {
    PLOG(ERROR) << "Failed to create cancel pipe";
    started->Signal();
    return;
  }

  proxy_fd_ = std::move(socket_fd);
  SetServiceLoopStatus(true);
  started->Signal();
  VLOG(1) << "CameraHalDispatcherImpl started; waiting for incoming connection";

  while (true) {
    if (!WaitForSocketReadable(proxy_fd_.get(), cancel_fd.get())) {
      VLOG(1) << "Quit CameraHalDispatcherImpl IO thread";
      SetServiceLoopStatus(false);
      return;
    }

    base::ScopedFD accepted_fd;
    if (mojo::AcceptSocketConnection(proxy_fd_.get(), &accepted_fd, false) &&
        accepted_fd.is_valid()) {
      VLOG(1) << "Accepted a connection";
      // Hardcode pid 0 since it is unused in mojo.
      const base::ProcessHandle kUnusedChildProcessHandle = 0;
      mojo::PlatformChannel channel;
      mojo::OutgoingInvitation invitation;

      // Generate an arbitrary 32-byte string, as we use this length as a
      // protocol version identifier.
      std::string token = GenerateRandomToken();
      mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(token);
      mojo::OutgoingInvitation::Send(std::move(invitation),
                                     kUnusedChildProcessHandle,
                                     channel.TakeLocalEndpoint());

      auto remote_endpoint = channel.TakeRemoteEndpoint();
      std::vector<base::ScopedFD> fds;
      fds.emplace_back(remote_endpoint.TakePlatformHandle().TakeFD());

      struct iovec iov = {const_cast<char*>(token.c_str()), token.length()};
      ssize_t result =
          mojo::SendmsgWithHandles(accepted_fd.get(), &iov, 1, fds);
      if (result == -1) {
        PLOG(ERROR) << "sendmsg()";
      } else {
        proxy_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&CameraHalDispatcherImpl::OnPeerConnected,
                                      base::Unretained(this), std::move(pipe)));
      }
    }
  }
}

void CameraHalDispatcherImpl::GetCameraSWPrivacySwitchStateOnProxyThread(
    cros::mojom::CrosCameraService::GetCameraSWPrivacySwitchStateCallback
        callback) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  if (!camera_service_.is_bound()) {
    LOG(ERROR) << "Camera HAL server is not registered";
    std::move(callback).Run(cros::mojom::CameraPrivacySwitchState::UNKNOWN);
    return;
  }
  camera_service_->GetCameraSWPrivacySwitchState(std::move(callback));
}

void CameraHalDispatcherImpl::SetCameraSWPrivacySwitchStateOnProxyThread(
    cros::mojom::CameraPrivacySwitchState state) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  if (!camera_service_.is_bound()) {
    LOG(ERROR) << "Camera HAL server is not registered";
    return;
  }
  camera_service_->SetCameraSWPrivacySwitchState(state);
}

void CameraHalDispatcherImpl::RegisterClientWithTokenOnProxyThread(
    mojo::PendingRemote<cros::mojom::CameraHalClient> client,
    cros::mojom::CameraClientType type,
    base::UnguessableToken auth_token,
    RegisterClientWithTokenCallback callback) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  auto client_observer = std::make_unique<MojoCameraClientObserver>(
      std::move(client), type, std::move(auth_token));
  client_observer->client().set_disconnect_handler(base::BindOnce(
      &CameraHalDispatcherImpl::OnCameraHalClientConnectionError,
      base::Unretained(this), base::Unretained(client_observer.get())));
  AddClientObserverOnProxyThread(client_observer.get(), std::move(callback),
                                 nullptr);
  mojo_client_observers_[client_observer.get()] = std::move(client_observer);
}

void CameraHalDispatcherImpl::AddClientObserverOnProxyThread(
    CameraClientObserver* observer,
    base::OnceCallback<void(int32_t)> result_callback,
    base::WaitableEvent* added) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  if (!observer->Authenticate(&token_manager_)) {
    LOG(ERROR) << "Failed to authenticate camera client observer";
    std::move(result_callback).Run(-EPERM);
    return;
  }
  if (camera_service_.is_bound()) {
    EstablishMojoChannel(observer);
  }
  // If the cros camera service is stopped, we just put it in the observer list.
  // The mojo channel will be established once the cros camera service starts.
  client_observers_.insert(observer);
  std::move(result_callback).Run(0);
  CAMERA_LOG(EVENT) << "Camera HAL client registered";
  if (added) {
    added->Signal();
  }
}

void CameraHalDispatcherImpl::EstablishMojoChannel(
    CameraClientObserver* client_observer) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  const auto& type = client_observer->GetType();
  CAMERA_LOG(EVENT) << "Establishing server channel for " << type;
  camera_service_->GetCameraModule(
      type, base::BindOnce(&CameraHalDispatcherImpl::OnGetCameraModule,
                           base::Unretained(this), client_observer));
}

void CameraHalDispatcherImpl::OnGetCameraModule(
    CameraClientObserver* client_observer,
    mojo::PendingRemote<cros::mojom::CameraModule> camera_module) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  if (client_observers_.find(client_observer) == client_observers_.end()) {
    return;
  }
  client_observer->OnChannelCreated(std::move(camera_module));
}

void CameraHalDispatcherImpl::OnPeerConnected(
    mojo::ScopedMessagePipeHandle message_pipe) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  receiver_set_.Add(this,
                    mojo::PendingReceiver<cros::mojom::CameraHalDispatcher>(
                        std::move(message_pipe)));
  VLOG(1) << "New CameraHalDispatcher binding added";
}

void CameraHalDispatcherImpl::OnCameraServiceConnectionError() {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock lock(opened_camera_id_map_lock_);
    CAMERA_LOG(EVENT) << "Camera HAL server connection lost";
    camera_service_.reset();
    camera_service_observer_receiver_.reset();
    for (auto& [camera_client_type, camera_id_set] : opened_camera_id_map_) {
      if (!camera_id_set.empty()) {
        active_client_observers_->Notify(
            FROM_HERE, &CameraActiveClientObserver::OnActiveClientChange,
            camera_client_type, /*is_new_active_client=*/false,
            /*active_device_ids=*/base::flat_set<std::string>());
      }
    }
    opened_camera_id_map_.clear();
  }

  {
    base::AutoLock lock(device_id_to_hw_privacy_switch_state_lock_);
    device_id_to_hw_privacy_switch_state_.clear();
  }
  privacy_switch_observers_->Notify(
      FROM_HERE,
      &CameraPrivacySwitchObserver::OnCameraHWPrivacySwitchStateChanged,
      std::string(), cros::mojom::CameraPrivacySwitchState::UNKNOWN);
}

void CameraHalDispatcherImpl::OnCameraHalClientConnectionError(
    CameraClientObserver* client_observer) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  CleanupClientOnProxyThread(client_observer);
}

void CameraHalDispatcherImpl::CleanupClientOnProxyThread(
    CameraClientObserver* client_observer) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(opened_camera_id_map_lock_);
  auto camera_client_type = client_observer->GetType();
  auto opened_it = opened_camera_id_map_.find(camera_client_type);
  if (opened_it != opened_camera_id_map_.end()) {
    const auto& camera_id_set = opened_it->second;
    if (!camera_id_set.empty()) {
      active_client_observers_->Notify(
          FROM_HERE, &CameraActiveClientObserver::OnActiveClientChange,
          camera_client_type,
          /*is_new_active_client=*/false,
          /*active_device_ids=*/base::flat_set<std::string>());
    }
    opened_camera_id_map_.erase(opened_it);
  }

  if (mojo_client_observers_.find(client_observer) !=
      mojo_client_observers_.end()) {
    mojo_client_observers_[client_observer].reset();
    mojo_client_observers_.erase(client_observer);
  }

  auto it = client_observers_.find(client_observer);
  if (it != client_observers_.end()) {
    client_observers_.erase(it);
    CAMERA_LOG(EVENT) << "Camera HAL client connection lost";
  }
}

void CameraHalDispatcherImpl::RemoveClientObserversOnProxyThread(
    std::vector<CameraClientObserver*> client_observers,
    base::WaitableEvent* removed) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  for (auto* client_observer : client_observers) {
    CleanupClientOnProxyThread(client_observer);
  }
  removed->Signal();
}

void CameraHalDispatcherImpl::RemoveClientObservers(
    std::vector<CameraClientObserver*> client_observers) {
  if (client_observers.empty()) {
    return;
  }
  DCHECK(proxy_thread_.IsRunning());
  base::WaitableEvent removed;
  proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImpl::RemoveClientObserversOnProxyThread,
          base::Unretained(this), client_observers,
          base::Unretained(&removed)));
  removed.Wait();
}

void CameraHalDispatcherImpl::StopOnProxyThread() {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());

  // TODO(crbug.com/1053569): Remove these lines once the issue is solved.
  base::File::Info info;
  if (!base::GetFileInfo(base::FilePath(kArcCamera3SocketPath), &info)) {
    LOG(WARNING) << "Failed to get socket info before deleting";
  } else {
    LOG(WARNING) << "Delete socket. Socket info:"
                 << " creation_time: " << info.creation_time
                 << " last_accessed: " << info.last_accessed
                 << " last_modified: " << info.last_modified;
  }

  if (!base::DeleteFile(base::FilePath(kArcCamera3SocketPath))) {
    LOG(ERROR) << "Failed to delete " << kArcCamera3SocketPath;
  }
  // Close |cancel_pipe_| to quit the loop in WaitForIncomingConnection.
  // If poll() is called after signaling |cancel_pipe_| without a while loop,
  // the service loop will deadlock because it will be blocked in poll() since
  // it is unable to receive the signal from |cancel_pipe_|. To avoid the
  // deadlock, |cancel_pipe_| is kept signaling until the service loop stops.
  while (IsServiceLoopRunning()) {
    cancel_pipe_.reset();
    base::PlatformThread::Sleep(base::Milliseconds(100));
  }

  mojo_client_observers_.clear();
  client_observers_.clear();
  camera_service_observer_receiver_.reset();
  camera_service_.reset();
  receiver_set_.Clear();
  {
    base::AutoLock lock(device_id_to_hw_privacy_switch_state_lock_);
    device_id_to_hw_privacy_switch_state_.clear();
  }
}

void CameraHalDispatcherImpl::SetAutoFramingState(
    cros::mojom::CameraAutoFramingState state) {
  if (!proxy_thread_.IsRunning()) {
    // The camera hal dispatcher is not running, ignore the request.
    // TODO(pihsun): Any better way?
    return;
  }
  proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImpl::SetAutoFramingStateOnProxyThread,
                     base::Unretained(this), state));
}

void CameraHalDispatcherImpl::SetAutoFramingStateOnProxyThread(
    cros::mojom::CameraAutoFramingState state) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());

  current_auto_framing_state_ = state;
  if (camera_service_.is_bound()) {
    camera_service_->SetAutoFramingState(state);
  }
}

void CameraHalDispatcherImpl::GetAutoFramingSupported(
    cros::mojom::CrosCameraService::GetAutoFramingSupportedCallback callback) {
  if (!proxy_thread_.IsRunning()) {
    std::move(callback).Run(false);
    return;
  }
  // Unretained reference is safe here because CameraHalDispatcherImpl owns
  // |proxy_thread_|.
  proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImpl::GetAutoFramingSupportedOnProxyThread,
          base::Unretained(this),
          // Make sure to hop back to the current thread for the reply.
          base::BindPostTaskToCurrentDefault(std::move(callback), FROM_HERE)));
}

void CameraHalDispatcherImpl::GetAutoFramingSupportedOnProxyThread(
    cros::mojom::CrosCameraService::GetAutoFramingSupportedCallback callback) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  if (!camera_service_.is_bound()) {
    // TODO(pihsun): Currently only AutozoomControllerImpl calls
    // GetAutoFramingSupported. Support multiple call to the function using
    // CallbackList if it's needed.
    DCHECK(!auto_framing_supported_callback_);
    auto_framing_supported_callback_ = std::move(callback);
    return;
  }
  camera_service_->GetAutoFramingSupported(std::move(callback));
}

void CameraHalDispatcherImpl::SetCameraEffects(
    cros::mojom::EffectsConfigPtr config) {
  if (!proxy_thread_.IsRunning()) {
    LOG(ERROR) << "CameraHalDispatcherImpl Error: calling SetCameraEffects "
                  "without proxy_thread_ running.";
    // The camera hal dispatcher is not running, ignore the request.
    // Notify with nullopt as the proxy thread is not running and camera effects
    // cannot be set in this case.
    camera_effect_observers_->Notify(
        FROM_HERE, &CameraEffectObserver::OnCameraEffectChanged,
        cros::mojom::EffectsConfigPtr());
    return;
  }

  proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImpl::SetCameraEffectsOnProxyThread,
                     base::Unretained(this), std::move(config),
                     /*is_from_register=*/false));
}

void CameraHalDispatcherImpl::SetCameraEffectsOnProxyThread(
    cros::mojom::EffectsConfigPtr config,
    bool is_from_register) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());

  if (camera_service_.is_bound()) {
    camera_service_->SetCameraEffect(
        config.Clone(),
        base::BindOnce(
            &CameraHalDispatcherImpl::OnSetCameraEffectsCompleteOnProxyThread,
            base::Unretained(this), config.Clone(), is_from_register));

  } else {
    // Save the config to initial_effects_ so that it will be applied when the
    // server becomes ready.
    initial_effects_ = std::move(config);

    LOG(ERROR)
        << "CameraHalDispatcherImpl Error: calling "
           "SetCameraEffectsOnProxyThread without camera server registered.";
    // Notify with nullopt as no camera server has been registered and camera
    // effects cannot be set in this case.
    camera_effect_observers_->Notify(
        FROM_HERE, &CameraEffectObserver::OnCameraEffectChanged,
        cros::mojom::EffectsConfigPtr());
  }
}

void CameraHalDispatcherImpl::OnSetCameraEffectsCompleteOnProxyThread(
    cros::mojom::EffectsConfigPtr config,
    bool is_from_register,
    cros::mojom::SetEffectResult result) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());

  cros::mojom::EffectsConfigPtr new_effects;
  // The new config is applied if set effects succeed. If the set effects fail,
  // no effects have been applied if the set is called from register and
  // the current effects do not change otherwise.
  //
  // The new config is applied if set effects succeed.
  if (result == cros::mojom::SetEffectResult::kOk) {
    new_effects = config.Clone();
  }
  // New config is not applied if set effects failed.
  else {
    LOG(ERROR) << "CameraHalDispatcherImpl Error: SetCameraEffectsComplete "
                  "returns with error code "
               << static_cast<int>(result);

    // If setting from register and failed, the new effects should be the
    // default effects.
    if (is_from_register) {
      new_effects = cros::mojom::EffectsConfig::New();
    }
    // If not setting from register, the new effects should still be the current
    // effects.
    else {
      new_effects = current_effects_.Clone();
    }
  }

  // Record the up-to-date camera effects.
  current_effects_ = new_effects.Clone();
  // Reset the `initial_effects_` if the `current_effects_` is not null.
  if (!current_effects_.is_null()) {
    initial_effects_.reset();
  }

  // Notify the camera effect configuration changes with the new effect.
  camera_effect_observers_->Notify(FROM_HERE,
                                   &CameraEffectObserver::OnCameraEffectChanged,
                                   std::move(new_effects));
}

void CameraHalDispatcherImpl::OnCameraEffectsObserverAddOnProxyThread(
    CameraEffectObserverCallback camera_effect_observer_callback) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  std::move(camera_effect_observer_callback).Run(current_effects_.Clone());
}

std::string CameraHalDispatcherImpl::GetDeviceIdFromCameraId(
    int32_t camera_id) {
  base::AutoLock lock(camera_id_to_device_id_lock_);
  auto it = camera_id_to_device_id_.find(camera_id);
  if (it == camera_id_to_device_id_.end()) {
    LOG(ERROR) << "Could not find device_id corresponding to camera_id: "
               << camera_id;
    return std::string();
  }
  return it->second;
}

base::flat_set<std::string> CameraHalDispatcherImpl::GetDeviceIdsFromCameraIds(
    base::flat_set<int32_t> camera_ids) {
  base::flat_set<std::string> device_ids;
  for (const auto& camera_id : camera_ids) {
    device_ids.insert(GetDeviceIdFromCameraId(camera_id));
  }
  return device_ids;
}

TokenManager* CameraHalDispatcherImpl::GetTokenManagerForTesting() {
  return &token_manager_;
}

void CameraHalDispatcherImpl::Request(
    chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
    mojo::ScopedMessagePipeHandle receiver) {
  // Unretained reference is safe here because CameraHalDispatcherImpl owns
  // |proxy_thread_|.
  proxy_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CameraHalDispatcherImpl::OnPeerConnected,
                                base::Unretained(this), std::move(receiver)));
  VLOG(1) << "New CameraHalDispatcher binding added from Mojo Service Manager.";
}

}  // namespace media
