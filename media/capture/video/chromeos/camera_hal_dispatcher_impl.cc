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
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/components/sensors/sensor_util.h"
#include "components/device_event_log/device_event_log.h"
#include "media/base/bind_to_current_loop.h"
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

namespace media {

namespace {

const base::FilePath::CharType kArcCamera3SocketPath[] =
    "/run/camera/camera3.sock";
const char kArcCameraGroup[] = "arc-camera";
const base::FilePath::CharType kForceEnableAePath[] =
    "/run/camera/force_enable_face_ae";
const base::FilePath::CharType kForceDisableAePath[] =
    "/run/camera/force_disable_face_ae";
const base::FilePath::CharType kForceEnableHdrNetPath[] =
    "/run/camera/force_enable_hdrnet";
const base::FilePath::CharType kForceDisableHdrNetPath[] =
    "/run/camera/force_disable_hdrnet";
const base::FilePath::CharType kForceEnableAutoFramingPath[] =
    "/run/camera/force_enable_auto_framing";
const base::FilePath::CharType kForceDisableAutoFramingPath[] =
    "/run/camera/force_disable_auto_framing";
const base::FilePath::CharType kForceEnableEffectsPath[] =
    "/run/camera/force_enable_effects";
const base::FilePath::CharType kForceDisableEffectsPath[] =
    "/run/camera/force_disable_effects";

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

FailedCameraHalServerCallbacks::FailedCameraHalServerCallbacks()
    : callbacks_(this) {}
FailedCameraHalServerCallbacks::~FailedCameraHalServerCallbacks() = default;

mojo::PendingRemote<cros::mojom::CameraHalServerCallbacks>
FailedCameraHalServerCallbacks::GetRemote() {
  return callbacks_.BindNewPipeAndPassRemote();
}

void FailedCameraHalServerCallbacks::CameraDeviceActivityChange(
    int32_t camera_id,
    bool opened,
    cros::mojom::CameraClientType type) {}

void FailedCameraHalServerCallbacks::CameraPrivacySwitchStateChange(
    cros::mojom::CameraPrivacySwitchState state,
    int32_t camera_id) {}

void FailedCameraHalServerCallbacks::CameraSWPrivacySwitchStateChange(
    cros::mojom::CameraPrivacySwitchState state) {}

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

bool CameraHalDispatcherImpl::Start(
    MojoMjpegDecodeAcceleratorFactoryCB jda_factory,
    MojoJpegEncodeAcceleratorFactoryCB jea_factory) {
  DCHECK(!IsStarted());
  if (!StartThreads()) {
    return false;
  }

  {
    base::FilePath enable_file_path(kForceEnableAePath);
    base::FilePath disable_file_path(kForceDisableAePath);
    if (!base::DeleteFile(enable_file_path)) {
      LOG(WARNING) << "Could not delete " << kForceEnableAePath;
    }
    if (!base::DeleteFile(disable_file_path)) {
      LOG(WARNING) << "Could not delete " << kForceDisableAePath;
    }
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(media::switches::kForceControlFaceAe)) {
      if (command_line->GetSwitchValueASCII(
              media::switches::kForceControlFaceAe) == "enable") {
        base::File file(enable_file_path, base::File::FLAG_CREATE_ALWAYS |
                                              base::File::FLAG_WRITE);
        file.Close();
      } else {
        base::File file(disable_file_path, base::File::FLAG_CREATE_ALWAYS |
                                               base::File::FLAG_WRITE);
        file.Close();
      }
    }
  }

  {
    base::FilePath enable_file_path(kForceEnableHdrNetPath);
    base::FilePath disable_file_path(kForceDisableHdrNetPath);
    if (!base::DeleteFile(enable_file_path)) {
      LOG(WARNING) << "Could not delete " << kForceEnableHdrNetPath;
    }
    if (!base::DeleteFile(disable_file_path)) {
      LOG(WARNING) << "Could not delete " << kForceDisableHdrNetPath;
    }
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(media::switches::kHdrNetOverride)) {
      std::string value =
          command_line->GetSwitchValueASCII(switches::kHdrNetOverride);
      if (value == switches::kHdrNetForceEnabled) {
        base::File file(enable_file_path, base::File::FLAG_CREATE_ALWAYS |
                                              base::File::FLAG_WRITE);
        file.Close();
      } else if (value == switches::kHdrNetForceDisabled) {
        base::File file(disable_file_path, base::File::FLAG_CREATE_ALWAYS |
                                               base::File::FLAG_WRITE);
        file.Close();
      }
    }
  }

  {
    base::FilePath enable_file_path(kForceEnableAutoFramingPath);
    base::FilePath disable_file_path(kForceDisableAutoFramingPath);
    if (!base::DeleteFile(enable_file_path)) {
      LOG(WARNING) << "Could not delete " << kForceEnableAutoFramingPath;
    }
    if (!base::DeleteFile(disable_file_path)) {
      LOG(WARNING) << "Could not delete " << kForceDisableAutoFramingPath;
    }
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(media::switches::kAutoFramingOverride)) {
      std::string value =
          command_line->GetSwitchValueASCII(switches::kAutoFramingOverride);
      if (value == switches::kAutoFramingForceEnabled) {
        base::File file(enable_file_path, base::File::FLAG_CREATE_ALWAYS |
                                              base::File::FLAG_WRITE);
        file.Close();
      } else if (value == switches::kAutoFramingForceDisabled) {
        base::File file(disable_file_path, base::File::FLAG_CREATE_ALWAYS |
                                               base::File::FLAG_WRITE);
        file.Close();
      }
    }
  }

  {
    base::FilePath enable_file_path(kForceEnableEffectsPath);
    base::FilePath disable_file_path(kForceDisableEffectsPath);
    if (!base::DeleteFile(enable_file_path)) {
      LOG(WARNING) << "Could not delete " << kForceEnableEffectsPath;
    }
    if (!base::DeleteFile(disable_file_path)) {
      LOG(WARNING) << "Could not delete " << kForceDisableEffectsPath;
    }
    base::File file(ash::features::IsVideoConferenceEnabled()
                        ? enable_file_path
                        : disable_file_path,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    file.Close();
  }

  jda_factory_ = std::move(jda_factory);
  jea_factory_ = std::move(jea_factory);
  base::WaitableEvent started;
  // It's important we generate tokens before creating the socket, because once
  // it is available, everyone connecting to socket would start fetching
  // tokens.
  if (!token_manager_.GenerateServerToken()) {
    LOG(ERROR) << "Failed to generate authentication token for server";
    return false;
  }
  if (HasCrosCameraTest() && !token_manager_.GenerateTestClientToken()) {
    LOG(ERROR) << "Failed to generate token for test client";
    return false;
  }
  if (!token_manager_.GenerateServerSensorClientToken()) {
    LOG(ERROR) << "Failed to generate authentication token for server as a "
                  "sensor client";
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

void CameraHalDispatcherImpl::GetCameraSWPrivacySwitchState(
    cros::mojom::CameraHalServer::GetCameraSWPrivacySwitchStateCallback
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

void CameraHalDispatcherImpl::DisableSensorForTesting() {
  sensor_enabled_ = false;
}

CameraHalDispatcherImpl::CameraHalDispatcherImpl()
    : proxy_thread_("CameraProxyThread"),
      blocking_io_thread_("CameraBlockingIOThread"),
      main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      camera_hal_server_callbacks_(this),
      active_client_observers_(
          new base::ObserverListThreadSafe<CameraActiveClientObserver>()),
      privacy_switch_observers_(
          new base::ObserverListThreadSafe<CameraPrivacySwitchObserver>()) {}

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

void CameraHalDispatcherImpl::RegisterServer(
    mojo::PendingRemote<cros::mojom::CameraHalServer> camera_hal_server) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  LOG(ERROR) << "CameraHalDispatcher::RegisterServer is deprecated. "
                "CameraHalServer will not be registered.";
}

void CameraHalDispatcherImpl::RegisterServerWithToken(
    mojo::PendingRemote<cros::mojom::CameraHalServer> camera_hal_server,
    const base::UnguessableToken& token,
    RegisterServerWithTokenCallback callback) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());

  if (camera_hal_server_) {
    LOG(ERROR) << "Camera HAL server is already registered";
    std::move(callback).Run(-EALREADY,
                            failed_camera_hal_server_callbacks_.GetRemote());
    return;
  }
  if (!token_manager_.AuthenticateServer(token)) {
    LOG(ERROR) << "Failed to authenticate server";
    std::move(callback).Run(-EPERM,
                            failed_camera_hal_server_callbacks_.GetRemote());
    return;
  }
  camera_hal_server_.Bind(std::move(camera_hal_server));
  camera_hal_server_.set_disconnect_handler(
      base::BindOnce(&CameraHalDispatcherImpl::OnCameraHalServerConnectionError,
                     base::Unretained(this)));
  if (auto_framing_supported_callback_) {
    camera_hal_server_->GetAutoFramingSupported(
        std::move(auto_framing_supported_callback_));
  }
  camera_hal_server_->SetAutoFramingState(current_auto_framing_state_);

  // Should only be called when an effect is set.
  if (!initial_effects_.is_null() || !current_effects_.is_null()) {
    // If current_effects_ is set, then a newer effect as applied since
    // the initial setup and we should use that, as the camera server
    // may have crashed and restarted.
    cros::mojom::EffectsConfigPtr& config =
        current_effects_.is_null() ? initial_effects_ : current_effects_;

    // There is a scenario where if the the camera server crashes and
    // restarts, and the SetCameraEffect fails, then current_effects_
    // will still show that an effect is enabled but the camera will
    // not have it set. Once the UI is implemented, we should reset
    // these variables so the user can notice it in the UI and manually
    // click the toggle to retrigger the effect. While we're still driving
    // these from chrome://flags, it's better to accept this edge case
    // so that the flag values will persist across camera crashes.
    //
    // initial_effects_.reset();
    // current_effects_.reset();

    camera_hal_server_->SetCameraEffect(
        config.Clone(),
        base::BindOnce(
            &CameraHalDispatcherImpl::OnSetCameraEffectsCompleteOnProxyThread,
            base::Unretained(this), config.Clone()));
  }

  CAMERA_LOG(EVENT) << "Camera HAL server registered";
  std::move(callback).Run(
      0, camera_hal_server_callbacks_.BindNewPipeAndPassRemote());

  // Set up the Mojo channels for clients which registered before the server
  // registers.
  for (auto* client_observer : client_observers_) {
    EstablishMojoChannel(client_observer);
  }
}

void CameraHalDispatcherImpl::RegisterClient(
    mojo::PendingRemote<cros::mojom::CameraHalClient> client) {
  NOTREACHED() << "RegisterClient() is disabled";
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
          media::BindToCurrentLoop(std::move(callback))));
}

void CameraHalDispatcherImpl::GetMjpegDecodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
        jda_receiver) {
  jda_factory_.Run(std::move(jda_receiver));
}

void CameraHalDispatcherImpl::GetJpegEncodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
        jea_receiver) {
  jea_factory_.Run(std::move(jea_receiver));
}

void CameraHalDispatcherImpl::RegisterSensorClientWithToken(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> client,
    const base::UnguessableToken& auth_token,
    RegisterSensorClientWithTokenCallback callback) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());

  if (!sensor_enabled_) {
    std::move(callback).Run(-EPERM);
    return;
  }

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImpl::RegisterSensorClientWithTokenOnUIThread,
          weak_factory_.GetWeakPtr(), std::move(client), auth_token,
          BindToCurrentLoop(std::move(callback))));
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
  started->Signal();
  VLOG(1) << "CameraHalDispatcherImpl started; waiting for incoming connection";

  while (true) {
    if (!WaitForSocketReadable(proxy_fd_.get(), cancel_fd.get())) {
      VLOG(1) << "Quit CameraHalDispatcherImpl IO thread";
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
    cros::mojom::CameraHalServer::GetCameraSWPrivacySwitchStateCallback
        callback) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  if (!camera_hal_server_) {
    LOG(ERROR) << "Camera HAL server is not registered";
    std::move(callback).Run(cros::mojom::CameraPrivacySwitchState::UNKNOWN);
    return;
  }
  camera_hal_server_->GetCameraSWPrivacySwitchState(std::move(callback));
}

void CameraHalDispatcherImpl::SetCameraSWPrivacySwitchStateOnProxyThread(
    cros::mojom::CameraPrivacySwitchState state) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  if (!camera_hal_server_) {
    LOG(ERROR) << "Camera HAL server is not registered";
    return;
  }
  camera_hal_server_->SetCameraSWPrivacySwitchState(state);
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
  if (camera_hal_server_) {
    EstablishMojoChannel(observer);
  }
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
  mojo::PendingRemote<cros::mojom::CameraModule> camera_module;
  const auto& type = client_observer->GetType();
  CAMERA_LOG(EVENT) << "Establishing server channel for " << type;
  camera_hal_server_->CreateChannel(
      camera_module.InitWithNewPipeAndPassReceiver(), type);
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

void CameraHalDispatcherImpl::OnCameraHalServerConnectionError() {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock lock(opened_camera_id_map_lock_);
    CAMERA_LOG(EVENT) << "Camera HAL server connection lost";
    camera_hal_server_.reset();
    camera_hal_server_callbacks_.reset();
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
  if (client_observers.empty())
    return;
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

void CameraHalDispatcherImpl::RegisterSensorClientWithTokenOnUIThread(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> client,
    const base::UnguessableToken& auth_token,
    RegisterSensorClientWithTokenCallback callback) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  if (!token_manager_.AuthenticateServerSensorClient(auth_token)) {
    std::move(callback).Run(-EPERM);
    return;
  }

  if (!chromeos::sensors::BindSensorHalClient(std::move(client))) {
    LOG(ERROR) << "Failed to bind SensorHalClient to SensorHalDispatcher";
    std::move(callback).Run(-ENOSYS);
    return;
  }

  std::move(callback).Run(0);
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
  cancel_pipe_.reset();
  mojo_client_observers_.clear();
  client_observers_.clear();
  camera_hal_server_callbacks_.reset();
  camera_hal_server_.reset();
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
  if (camera_hal_server_) {
    camera_hal_server_->SetAutoFramingState(state);
  }
}

void CameraHalDispatcherImpl::GetAutoFramingSupported(
    cros::mojom::CameraHalServer::GetAutoFramingSupportedCallback callback) {
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
          base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                             std::move(callback), FROM_HERE)));
}

void CameraHalDispatcherImpl::GetAutoFramingSupportedOnProxyThread(
    cros::mojom::CameraHalServer::GetAutoFramingSupportedCallback callback) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  if (!camera_hal_server_) {
    // TODO(pihsun): Currently only AutozoomControllerImpl calls
    // GetAutoFramingSupported. Support multiple call to the function using
    // CallbackList if it's needed.
    DCHECK(!auto_framing_supported_callback_);
    auto_framing_supported_callback_ = std::move(callback);
    return;
  }
  camera_hal_server_->GetAutoFramingSupported(std::move(callback));
}

void CameraHalDispatcherImpl::SetCameraEffectsControllerCallback(
    CameraHalDispatcherImpl::CameraEffectsControllerCallback
        camera_effects_controller_callback) {
  camera_effects_controller_callback_ =
      std::move(camera_effects_controller_callback);
}

void CameraHalDispatcherImpl::SetInitialCameraEffects(
    cros::mojom::EffectsConfigPtr config) {
  if (!proxy_thread_.IsRunning()) {
    // The camera hal dispatcher is not running, ignore the request.
    return;
  }
  proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraHalDispatcherImpl::SetInitialCameraEffectsOnProxyThread,
          base::Unretained(this), std::move(config)));
}

void CameraHalDispatcherImpl::SetInitialCameraEffectsOnProxyThread(
    cros::mojom::EffectsConfigPtr config) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  initial_effects_ = std::move(config);
}

void CameraHalDispatcherImpl::SetCameraEffects(
    cros::mojom::EffectsConfigPtr config) {
  // `camera_effects_controller_callback_` should be set before calling
  // SetCameraEffects.
  if (camera_effects_controller_callback_.is_null())
    return;

  if (!proxy_thread_.IsRunning()) {
    // The camera hal dispatcher is not running, ignore the request.
    camera_effects_controller_callback_.Run(
        current_effects_.Clone(), cros::mojom::SetEffectResult::kError);
    return;
  }

  proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImpl::SetCameraEffectsOnProxyThread,
                     base::Unretained(this), std::move(config)));
}

void CameraHalDispatcherImpl::GetCameraEffects(
    VideoCaptureDevice::GetPhotoStateCallback callback,
    media::mojom::PhotoStatePtr photo_state) {
  // All calls must be on the `proxy_task_runner`, so re-post the call if
  // needed.
  if (!proxy_task_runner_->BelongsToCurrentThread()) {
    proxy_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CameraHalDispatcherImpl::GetCameraEffects,
                                  base::Unretained(this),
                                  media::BindToCurrentLoop(std::move(callback)),
                                  std::move(photo_state)));
    return;
  }

  DCHECK(proxy_task_runner_->BelongsToCurrentThread());

  if (!current_effects_.is_null()) {
    photo_state->supported_background_blur_modes = {
        mojom::BackgroundBlurMode::BLUR, mojom::BackgroundBlurMode::OFF};

    photo_state->background_blur_mode = current_effects_->blur_enabled
                                            ? mojom::BackgroundBlurMode::BLUR
                                            : mojom::BackgroundBlurMode::OFF;
  }

  std::move(callback).Run(std::move(photo_state));
}

void CameraHalDispatcherImpl::SetCameraEffectsOnProxyThread(
    cros::mojom::EffectsConfigPtr config) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());

  if (camera_hal_server_) {
    camera_hal_server_->SetCameraEffect(
        config.Clone(),
        base::BindOnce(
            &CameraHalDispatcherImpl::OnSetCameraEffectsCompleteOnProxyThread,
            base::Unretained(this), config.Clone()));

  } else {
    LOG(ERROR) << "Cannot change camera effects, no camera server registered.";
    OnSetCameraEffectsCompleteOnProxyThread(
        std::move(config), cros::mojom::SetEffectResult::kError);
  }
}

void CameraHalDispatcherImpl::OnSetCameraEffectsCompleteOnProxyThread(
    cros::mojom::EffectsConfigPtr config,
    cros::mojom::SetEffectResult result) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());

  // Directly return if SetCameraEffect failed.
  if (result == cros::mojom::SetEffectResult::kError) {
    LOG(ERROR) << "SetCameraEffect failed.";
    camera_effects_controller_callback_.Run(
        current_effects_.Clone(), cros::mojom::SetEffectResult::kError);
    return;
  }

  // Record latest successful camera effects.
  current_effects_ = std::move(config);

  camera_effects_controller_callback_.Run(current_effects_.Clone(),
                                          cros::mojom::SetEffectResult::kOk);
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

}  // namespace media
