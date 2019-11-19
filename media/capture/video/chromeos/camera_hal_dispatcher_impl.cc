// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <sys/uio.h>

#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
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
      {raw_socket_fd, POLLIN, 0}, {raw_cancel_fd, POLLIN, 0},
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

class MojoCameraClientObserver : public CameraClientObserver {
 public:
  explicit MojoCameraClientObserver(
      mojo::PendingRemote<cros::mojom::CameraHalClient> client)
      : client_(std::move(client)) {}

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
  jda_factory_ = std::move(jda_factory);
  jea_factory_ = std::move(jea_factory);
  base::WaitableEvent started(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
  blocking_io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImpl::CreateSocket,
                     base::Unretained(this), base::Unretained(&started)));
  started.Wait();
  return IsStarted();
}

void CameraHalDispatcherImpl::AddClientObserver(
    std::unique_ptr<CameraClientObserver> observer) {
  // If |proxy_thread_| fails to start in Start() then CameraHalDelegate will
  // not be created, and this function will not be called.
  DCHECK(proxy_thread_.IsRunning());
  proxy_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImpl::AddClientObserverOnProxyThread,
                     base::Unretained(this), base::Passed(&observer)));
}

bool CameraHalDispatcherImpl::IsStarted() {
  return proxy_thread_.IsRunning() && blocking_io_thread_.IsRunning() &&
         proxy_fd_.is_valid();
}

CameraHalDispatcherImpl::CameraHalDispatcherImpl()
    : proxy_thread_("CameraProxyThread"),
      blocking_io_thread_("CameraBlockingIOThread") {
  // This event is for adding camera category to categories list.
  TRACE_EVENT0("camera", "CameraHalDispatcherImpl");
  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(this);
}

CameraHalDispatcherImpl::~CameraHalDispatcherImpl() {
  VLOG(1) << "Stopping CameraHalDispatcherImpl...";
  if (proxy_thread_.IsRunning()) {
    proxy_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&CameraHalDispatcherImpl::StopOnProxyThread,
                                  base::Unretained(this)));
    proxy_thread_.Stop();
  }
  blocking_io_thread_.Stop();
  base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
  VLOG(1) << "CameraHalDispatcherImpl stopped";
}

void CameraHalDispatcherImpl::RegisterServer(
    mojo::PendingRemote<cros::mojom::CameraHalServer> camera_hal_server) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());

  if (camera_hal_server_) {
    LOG(ERROR) << "Camera HAL server is already registered";
    return;
  }
  camera_hal_server_.Bind(std::move(camera_hal_server));
  camera_hal_server_.set_disconnect_handler(
      base::BindOnce(&CameraHalDispatcherImpl::OnCameraHalServerConnectionError,
                     base::Unretained(this)));
  VLOG(1) << "Camera HAL server registered";

  // Set up the Mojo channels for clients which registered before the server
  // registers.
  for (auto& client_observer : client_observers_) {
    EstablishMojoChannel(client_observer.get());
  }
}

void CameraHalDispatcherImpl::RegisterClient(
    mojo::PendingRemote<cros::mojom::CameraHalClient> client) {
  // RegisterClient can be called locally by ArcCameraBridge. Unretained
  // reference is safe here because CameraHalDispatcherImpl owns
  // |proxy_thread_|.
  proxy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDispatcherImpl::RegisterClientOnProxyThread,
                     base::Unretained(this), std::move(client)));
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
            FROM_HERE,
            base::BindOnce(&CameraHalDispatcherImpl::OnPeerConnected,
                           base::Unretained(this), base::Passed(&pipe)));
      }
    }
  }
}

void CameraHalDispatcherImpl::RegisterClientOnProxyThread(
    mojo::PendingRemote<cros::mojom::CameraHalClient> client) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  auto client_observer =
      std::make_unique<MojoCameraClientObserver>(std::move(client));
  client_observer->client().set_disconnect_handler(base::BindOnce(
      &CameraHalDispatcherImpl::OnCameraHalClientConnectionError,
      base::Unretained(this), base::Unretained(client_observer.get())));
  AddClientObserver(std::move(client_observer));
  VLOG(1) << "Camera HAL client registered";
}

void CameraHalDispatcherImpl::AddClientObserverOnProxyThread(
    std::unique_ptr<CameraClientObserver> observer) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  if (camera_hal_server_) {
    EstablishMojoChannel(observer.get());
  }
  client_observers_.insert(std::move(observer));
}

void CameraHalDispatcherImpl::EstablishMojoChannel(
    CameraClientObserver* client_observer) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  mojo::PendingRemote<cros::mojom::CameraModule> camera_module;
  camera_hal_server_->CreateChannel(
      camera_module.InitWithNewPipeAndPassReceiver());
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
  VLOG(1) << "Camera HAL server connection lost";
  camera_hal_server_.reset();
}

void CameraHalDispatcherImpl::OnCameraHalClientConnectionError(
    CameraClientObserver* client_observer) {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  auto it = client_observers_.find(client_observer);
  if (it != client_observers_.end()) {
    client_observers_.erase(it);
    VLOG(1) << "Camera HAL client connection lost";
  }
}

void CameraHalDispatcherImpl::StopOnProxyThread() {
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  if (!base::DeleteFile(base::FilePath(kArcCamera3SocketPath),
                        /* recursive */ false)) {
    LOG(ERROR) << "Failed to delete " << kArcCamera3SocketPath;
  }
  // Close |cancel_pipe_| to quit the loop in WaitForIncomingConnection.
  cancel_pipe_.reset();
  client_observers_.clear();
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

}  // namespace media
