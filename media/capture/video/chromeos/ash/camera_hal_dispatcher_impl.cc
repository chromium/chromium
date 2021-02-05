// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/ash/camera_hal_dispatcher_impl.h"
#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <sys/uio.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "components/device_event_log/device_event_log.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/chromeos/mojom/camera_common.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"
#include "mojo/public/cpp/system/invitation.h"

namespace media {

namespace {

const base::FilePath::CharType kArcCamera3SocketPath[] =
    "/var/run/camera/camera3.sock";
const char kArcCameraGroup[] = "arc-camera";

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

  if (HANDLE_EINTR(poll(fds, base::size(fds), -1)) <= 0) {
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
  explicit MojoCameraClientObserver(
      mojo::PendingRemote<cros::mojom::CameraHalClient> client,
      cros::mojom::CameraClientType type,
      base::UnguessableToken auth_token)
      : CameraClientObserver(type, std::move(auth_token)),
        client_(std::move(client)) {}

  void OnChannelCreated(
      mojo::PendingRemote<cros::mojom::CameraModule> camera_module) override {
    client_->SetUpChannel(std::move(camera_module));
  }

  mojo::Remote<cros::mojom::CameraHalClient>& client() { return client_; }

 private:
  mojo::Remote<cros::mojom::CameraHalClient> client_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(MojoCameraClientObserver);
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
  // This event is for adding camera category to categories list.
  TRACE_EVENT0("camera", "CameraHalDispatcherImpl");
  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(this);

  jda_factory_ = std::move(jda_factory);
  jea_factory_ = std::move(jea_factory);
  base::WaitableEvent started(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
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
  blocking_io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImpl::CreateSocket,
                     base::Unretained(this), base::Unretained(&started)));
  started.Wait();
  return IsStarted();
}

void CameraHalDispatcherImpl::AddClientObserver(
    std::unique_ptr<CameraClientObserver> observer,
    base::OnceCallback<void(int32_t)> result_callback) {
  // If |proxy_thread_| fails to start in Start() then CameraHalDelegate will
  // not be created, and this function will not be called.
  DCHECK(proxy_thread_.IsRunning());
  proxy_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImpl::AddClientObserverOnProxyThread,
                     base::Unretained(this), std::move(observer),
                     std::move(result_callback)));
}

bool CameraHalDispatcherImpl::IsStarted() {
  return proxy_thread_.IsRunning() && blocking_io_thread_.IsRunning() &&
         proxy_fd_.is_valid();
}

void CameraHalDispatcherImpl::AddActiveClientObserver(
    CameraActiveClientObserver* observer) {
  base::AutoLock lock(opened_camera_id_map_lock_);
  for (auto& opened_camera_id_pair : opened_camera_id_map_) {
    const auto& camera_client_type = opened_camera_id_pair.first;
    const auto& camera_id_set = opened_camera_id_pair.second;
    if (!camera_id_set.empty()) {
      observer->OnActiveClientChange(camera_client_type, /*is_active=*/true);
    }
  }
  active_client_observers_->AddObserver(observer);
}

void CameraHalDispatcherImpl::RemoveActiveClientObserver(
    CameraActiveClientObserver* observer) {
  active_client_observers_->RemoveObserver(observer);
}

cros::mojom::CameraPrivacySwitchState
CameraHalDispatcherImpl::AddCameraPrivacySwitchObserver(
    CameraPrivacySwitchObserver* observer) {
  privacy_switch_observers_->AddObserver(observer);

  base::AutoLock lock(privacy_switch_state_lock_);
  return current_privacy_switch_state_;
}

void CameraHalDispatcherImpl::RemoveCameraPrivacySwitchObserver(
    CameraPrivacySwitchObserver* observer) {
  privacy_switch_observers_->RemoveObserver(observer);
}

void CameraHalDispatcherImpl::RegisterPluginVmToken(
    const base::UnguessableToken& token) {
  token_manager_.RegisterPluginVmToken(token);
}

void CameraHalDispatcherImpl::UnregisterPluginVmToken(
    const base::UnguessableToken& token) {
  token_manager_.UnregisterPluginVmToken(token);
}

CameraHalDispatcherImpl::CameraHalDispatcherImpl()
    : proxy_thread_("CameraProxyThread"),
      blocking_io_thread_("CameraBlockingIOThread"),
      camera_hal_server_callbacks_(this),
      active_client_observers_(
          new base::ObserverListThreadSafe<CameraActiveClientObserver>()),
      current_privacy_switch_state_(
          cros::mojom::CameraPrivacySwitchState::UNKNOWN),
      privacy_switch_observers_(
          new base::ObserverListThreadSafe<CameraPrivacySwitchObserver>()) {}

CameraHalDispatcherImpl::~CameraHalDispatcherImpl() {
  VLOG(1) << "Stopping CameraHalDispatcherImpl...";
  if (proxy_thread_.IsRunning()) {
    proxy_thread_.task_runner()->PostTask(
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
  CAMERA_LOG(EVENT) << "Camera HAL server registered";
  std::move(callback).Run(
      0, camera_hal_server_callbacks_.BindNewPipeAndPassRemote());

  // Set up the Mojo channels for clients which registered before the server
  // registers.
  for (auto& client_observer : client_observers_) {
    EstablishMojoChannel(client_observer.get());
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

void CameraHalDispatcherImpl::GetJpegDecodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
        jda_receiver) {
  jda_factory_.Run(std::move(jda_receiver));
}

void CameraHalDispatcherImpl::GetJpegEncodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
        jea_receiver) {
  jea_factory_.Run(std::move(jea_receiver));
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
      active_client_observers_->Notify(
          FROM_HERE, &CameraActiveClientObserver::OnActiveClientChange, type,
          /*is_active=*/true);
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
      active_client_observers_->Notify(
          FROM_HERE, &CameraActiveClientObserver::OnActiveClientChange, type,
          /*is_active=*/false);
    }
  }
}

void CameraHalDispatcherImpl::CameraPrivacySwitchStateChange(
    cros::mojom::CameraPrivacySwitchState state) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(privacy_switch_state_lock_);
  current_privacy_switch_state_ = state;
  privacy_switch_observers_->Notify(
      FROM_HERE,
      &CameraPrivacySwitchObserver::OnCameraPrivacySwitchStatusChanged,
      current_privacy_switch_state_);
}

base::UnguessableToken CameraHalDispatcherImpl::GetTokenForTrustedClient(
    cros::mojom::CameraClientType type) {
  return token_manager_.GetTokenForTrustedClient(type);
}

void CameraHalDispatcherImpl::OnTraceLogEnabled() {
  proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImpl::OnTraceLogEnabledOnProxyThread,
                     base::Unretained(this)));
}

void CameraHalDispatcherImpl::OnTraceLogDisabled() {
  proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImpl::OnTraceLogDisabledOnProxyThread,
                     base::Unretained(this)));
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
  AddClientObserverOnProxyThread(std::move(client_observer),
                                 std::move(callback));
}

void CameraHalDispatcherImpl::AddClientObserverOnProxyThread(
    std::unique_ptr<CameraClientObserver> observer,
    base::OnceCallback<void(int32_t)> result_callback) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  if (!observer->Authenticate(&token_manager_)) {
    LOG(ERROR) << "Failed to authenticate camera client observer";
    std::move(result_callback).Run(-EPERM);
    return;
  }
  if (camera_hal_server_) {
    EstablishMojoChannel(observer.get());
  }
  client_observers_.insert(std::move(observer));
  std::move(result_callback).Run(0);
  CAMERA_LOG(EVENT) << "Camera HAL client registered";
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
  base::AutoLock lock(opened_camera_id_map_lock_);
  CAMERA_LOG(EVENT) << "Camera HAL server connection lost";
  camera_hal_server_.reset();
  camera_hal_server_callbacks_.reset();
  for (auto& opened_camera_id_pair : opened_camera_id_map_) {
    auto camera_client_type = opened_camera_id_pair.first;
    const auto& camera_id_set = opened_camera_id_pair.second;
    if (!camera_id_set.empty()) {
      active_client_observers_->Notify(
          FROM_HERE, &CameraActiveClientObserver::OnActiveClientChange,
          camera_client_type, /*is_active=*/false);
    }
  }
  opened_camera_id_map_.clear();

  base::AutoLock privacy_lock(privacy_switch_state_lock_);
  current_privacy_switch_state_ =
      cros::mojom::CameraPrivacySwitchState::UNKNOWN;
  privacy_switch_observers_->Notify(
      FROM_HERE,
      &CameraPrivacySwitchObserver::OnCameraPrivacySwitchStatusChanged,
      current_privacy_switch_state_);
}

void CameraHalDispatcherImpl::OnCameraHalClientConnectionError(
    CameraClientObserver* client_observer) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(opened_camera_id_map_lock_);
  auto camera_client_type = client_observer->GetType();
  auto opened_it = opened_camera_id_map_.find(camera_client_type);
  if (opened_it == opened_camera_id_map_.end()) {
    // This can happen if this camera client never opened a camera.
    return;
  }
  const auto& camera_id_set = opened_it->second;
  if (!camera_id_set.empty()) {
    active_client_observers_->Notify(
        FROM_HERE, &CameraActiveClientObserver::OnActiveClientChange,
        camera_client_type, /*is_active=*/false);
  }
  opened_camera_id_map_.erase(opened_it);

  auto it = client_observers_.find(client_observer);
  if (it != client_observers_.end()) {
    client_observers_.erase(it);
    CAMERA_LOG(EVENT) << "Camera HAL client connection lost";
  }
}

void CameraHalDispatcherImpl::StopOnProxyThread() {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(this);

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
  client_observers_.clear();
  camera_hal_server_callbacks_.reset();
  camera_hal_server_.reset();
  receiver_set_.Clear();
}

void CameraHalDispatcherImpl::OnTraceLogEnabledOnProxyThread() {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  if (!camera_hal_server_) {
    return;
  }
  bool camera_event_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("camera", &camera_event_enabled);
  if (camera_event_enabled) {
    camera_hal_server_->SetTracingEnabled(true);
  }
}

void CameraHalDispatcherImpl::OnTraceLogDisabledOnProxyThread() {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  if (!camera_hal_server_) {
    return;
  }
  camera_hal_server_->SetTracingEnabled(false);
}

TokenManager* CameraHalDispatcherImpl::GetTokenManagerForTesting() {
  return &token_manager_;
}

}  // namespace media
