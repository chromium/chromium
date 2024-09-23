// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_win.h"

#include <objbase.h>

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "device/bluetooth/bluetooth_device_win.h"
#include "device/bluetooth/bluetooth_init_win.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/winsock_init.h"
#include "net/base/winsock_util.h"
#include "net/log/net_log_source.h"

namespace {

const char kL2CAPNotSupported[] = "Bluetooth L2CAP protocal is not supported";
const char kSocketAlreadyConnected[] = "Socket is already connected.";
const char kInvalidRfcommPort[] = "Invalid RFCCOMM port.";
const char kFailedToCreateSocket[] = "Failed to create socket.";
const char kFailedToBindSocket[] = "Failed to bind socket.";
const char kFailedToListenOnSocket[] = "Failed to listen on socket.";
const char kFailedToGetSockNameForSocket[] = "Failed to getsockname.";
const char kFailedToAccept[] = "Failed to accept.";
const char kInvalidUUID[] = "Invalid UUID";
const char kWsaSetServiceError[] = "WSASetService error.";

std::string IPEndPointToBluetoothAddress(const net::IPEndPoint& end_point) {
  if (end_point.address().size() != net::kBluetoothAddressSize)
    return std::string();
  // The address is copied from BTH_ADDR field of SOCKADDR_BTH, which is a
  // 64-bit ULONGLONG that stores Bluetooth address in little-endian. Print in
  // reverse order to preserve the correct ordering.
  return base::StringPrintf(
      "%02X:%02X:%02X:%02X:%02X:%02X", end_point.address().bytes()[5],
      end_point.address().bytes()[4], end_point.address().bytes()[3],
      end_point.address().bytes()[2], end_point.address().bytes()[1],
      end_point.address().bytes()[0]);
}

}  // namespace

namespace device {

struct BluetoothSocketWin::ServiceRegData {
  ServiceRegData() {
    ZeroMemory(&address, sizeof(address));
    ZeroMemory(&address_info, sizeof(address_info));
    ZeroMemory(&uuid, sizeof(uuid));
    ZeroMemory(&service, sizeof(service));
  }

  SOCKADDR_BTH address;
  CSADDR_INFO address_info;
  GUID uuid;
  std::u16string name;
  WSAQUERYSET service;
};

// static
scoped_refptr<BluetoothSocketWin>
BluetoothSocketWin::CreateBluetoothSocket(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<device::BluetoothSocketThread> socket_thread) {
  DCHECK(ui_task_runner->RunsTasksInCurrentSequence());

  return base::WrapRefCounted(
      new BluetoothSocketWin(ui_task_runner, socket_thread));
}

BluetoothSocketWin::BluetoothSocketWin(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<BluetoothSocketThread> socket_thread)
    : BluetoothSocketNet(ui_task_runner, socket_thread),
      supports_rfcomm_(false),
      rfcomm_channel_(0xFF),
      bth_addr_(BTH_ADDR_NULL) {
}

BluetoothSocketWin::~BluetoothSocketWin() {
}

void BluetoothSocketWin::Connect(const BluetoothDeviceWin* device,
                                 const BluetoothUUID& uuid,
                                 base::OnceClosure success_callback,
                                 ErrorCompletionCallback error_callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(device);

  if (!uuid.IsValid()) {
    std::move(error_callback).Run(kInvalidUUID);
    return;
  }

  const BluetoothServiceRecordWin* service_record_win =
      device->GetServiceRecord(uuid);
  if (!service_record_win) {
    std::move(error_callback).Run(kInvalidUUID);
    return;
  }

  device_address_ = service_record_win->device_address();
  if (service_record_win->SupportsRfcomm()) {
    supports_rfcomm_ = true;
    rfcomm_channel_ = service_record_win->rfcomm_channel();
    bth_addr_ = service_record_win->device_bth_addr();
  }

  socket_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSocketWin::DoConnect, this,
                     base::BindOnce(&BluetoothSocketWin::PostSuccess, this,
                                    std::move(success_callback)),
                     base::BindOnce(&BluetoothSocketWin::PostErrorCompletion,
                                    this, std::move(error_callback))));
}

void BluetoothSocketWin::Listen(scoped_refptr<BluetoothAdapter> adapter,
                                const BluetoothUUID& uuid,
                                const BluetoothAdapter::ServiceOptions& options,
                                base::OnceClosure success_callback,
                                ErrorCompletionCallback error_callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  adapter_ = adapter;
  int rfcomm_channel = options.channel ? *options.channel : 0;

  // TODO(xiyuan): Use |options.name|.
  socket_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSocketWin::DoListen, this, uuid, rfcomm_channel,
                     std::move(success_callback), std::move(error_callback)));
}

void BluetoothSocketWin::ResetData() {
  if (service_reg_data_) {
    if (WSASetService(&service_reg_data_->service,RNRSERVICE_DELETE, 0) ==
        SOCKET_ERROR) {
      LOG(WARNING) << "Failed to unregister service.";
    }
    service_reg_data_.reset();
  }
}

void BluetoothSocketWin::Accept(AcceptCompletionCallback success_callback,
                                ErrorCompletionCallback error_callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  socket_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSocketWin::DoAccept, this,
                     std::move(success_callback), std::move(error_callback)));
}

void BluetoothSocketWin::DoConnect(base::OnceClosure success_callback,
                                   ErrorCompletionCallback error_callback) {
  DCHECK(socket_thread()->task_runner()->RunsTasksInCurrentSequence());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (tcp_socket()) {
    std::move(error_callback).Run(kSocketAlreadyConnected);
    return;
  }

  if (!supports_rfcomm_) {
    // TODO(youngki) add support for L2CAP sockets as well.
    std::move(error_callback).Run(kL2CAPNotSupported);
    return;
  }

  std::unique_ptr<net::TCPSocket> scoped_socket =
      net::TCPSocket::Create(nullptr, nullptr, net::NetLogSource());
  net::EnsureWinsockInit();
  SOCKET socket_fd = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
  SOCKADDR_BTH sa;
  ZeroMemory(&sa, sizeof(sa));
  sa.addressFamily = AF_BTH;
  sa.port = rfcomm_channel_;
  sa.btAddr = bth_addr_;

  // TODO(rpaquay): Condider making this call non-blocking.
  int status = connect(socket_fd, reinterpret_cast<SOCKADDR*>(&sa), sizeof(sa));
  DWORD error_code = WSAGetLastError();
  if (!(status == 0 || error_code == WSAEINPROGRESS)) {
    LOG(ERROR) << "Failed to connect bluetooth socket "
               << "(" << device_address_ << "): "
               << logging::SystemErrorCodeToString(error_code);
    std::move(error_callback)
        .Run("Error connecting to socket: " +
             logging::SystemErrorCodeToString(error_code));
    closesocket(socket_fd);
    return;
  }

  // Note: We don't have a meaningful |IPEndPoint|, but that is ok since the
  // TCPSocket implementation does not actually require one.
  int net_result =
      scoped_socket->AdoptConnectedSocket(socket_fd, net::IPEndPoint());
  if (net_result != net::OK) {
    std::move(error_callback)
        .Run("Error connecting to socket: " + net::ErrorToString(net_result));
    closesocket(socket_fd);
    return;
  }

  SetTCPSocket(std::move(scoped_socket));
  std::move(success_callback).Run();
}

void BluetoothSocketWin::DoListen(const BluetoothUUID& uuid,
                                  int rfcomm_channel,
                                  base::OnceClosure success_callback,
                                  ErrorCompletionCallback error_callback) {
  DCHECK(socket_thread()->task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!tcp_socket() && !service_reg_data_);

  // The valid range is 0-30. 0 means BT_PORT_ANY and 1-30 are the
  // valid RFCOMM port numbers of SOCKADDR_BTH.
  if (rfcomm_channel < 0 || rfcomm_channel > 30) {
    LOG(WARNING) << "Failed to start service: "
                 << "Invalid RFCCOMM port " << rfcomm_channel
                 << ", uuid=" << uuid.value();
    PostErrorCompletion(std::move(error_callback), kInvalidRfcommPort);
    return;
  }

  SOCKET socket_fd = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
  if (socket_fd == INVALID_SOCKET) {
    LOG(WARNING) << "Failed to start service: create socket, "
                 << "winsock err=" << WSAGetLastError();
    PostErrorCompletion(std::move(error_callback), kFailedToCreateSocket);
    return;
  }

  // Note that |socket_fd| belongs to a non-TCP address family (i.e. AF_BTH),
  // TCPSocket methods that involve address could not be called. So bind()
  // is called on |socket_fd| directly.
  std::unique_ptr<net::TCPSocket> scoped_socket =
      net::TCPSocket::Create(nullptr, nullptr, net::NetLogSource());
  scoped_socket->AdoptUnconnectedSocket(socket_fd);

  SOCKADDR_BTH sa;
  struct sockaddr* sock_addr = reinterpret_cast<struct sockaddr*>(&sa);
  int sock_addr_len = sizeof(sa);
  ZeroMemory(&sa, sock_addr_len);
  sa.addressFamily = AF_BTH;
  sa.port = rfcomm_channel ? rfcomm_channel : BT_PORT_ANY;
  if (bind(socket_fd, sock_addr, sock_addr_len) < 0) {
    LOG(WARNING) << "Failed to start service: create socket, "
                 << "winsock err=" << WSAGetLastError();
    PostErrorCompletion(std::move(error_callback), kFailedToBindSocket);
    return;
  }

  const int kListenBacklog = 5;
  if (scoped_socket->Listen(kListenBacklog) < 0) {
    LOG(WARNING) << "Failed to start service: Listen"
                 << "winsock err=" << WSAGetLastError();
    PostErrorCompletion(std::move(error_callback), kFailedToListenOnSocket);
    return;
  }

  std::unique_ptr<ServiceRegData> reg_data(new ServiceRegData);
  reg_data->name = base::UTF8ToUTF16(uuid.canonical_value());

  if (getsockname(socket_fd, sock_addr, &sock_addr_len)) {
    LOG(WARNING) << "Failed to start service: getsockname, "
                 << "winsock err=" << WSAGetLastError();
    PostErrorCompletion(std::move(error_callback),
                        kFailedToGetSockNameForSocket);
    return;
  }
  reg_data->address = sa;

  reg_data->address_info.LocalAddr.iSockaddrLength = sizeof(sa);
  reg_data->address_info.LocalAddr.lpSockaddr =
      reinterpret_cast<struct sockaddr*>(&reg_data->address);
  reg_data->address_info.iSocketType = SOCK_STREAM;
  reg_data->address_info.iProtocol = BTHPROTO_RFCOMM;

  std::u16string cannonical_uuid =
      u"{" + base::ASCIIToUTF16(uuid.canonical_value()) + u"}";
  if (!SUCCEEDED(
          CLSIDFromString(base::as_wcstr(cannonical_uuid), &reg_data->uuid))) {
    LOG(WARNING) << "Failed to start service: "
                 << ", invalid uuid=" << cannonical_uuid;
    PostErrorCompletion(std::move(error_callback), kInvalidUUID);
    return;
  }

  reg_data->service.dwSize = sizeof(WSAQUERYSET);
  reg_data->service.lpszServiceInstanceName =
      base::as_writable_wcstr(reg_data->name);
  reg_data->service.lpServiceClassId = &reg_data->uuid;
  reg_data->service.dwNameSpace = NS_BTH;
  reg_data->service.dwNumberOfCsAddrs = 1;
  reg_data->service.lpcsaBuffer = &reg_data->address_info;

  if (WSASetService(&reg_data->service,
                    RNRSERVICE_REGISTER, 0) == SOCKET_ERROR) {
    LOG(WARNING) << "Failed to register profile: WSASetService"
                 << "winsock err=" << WSAGetLastError();
    PostErrorCompletion(std::move(error_callback), kWsaSetServiceError);
    return;
  }

  SetTCPSocket(std::move(scoped_socket));
  service_reg_data_ = std::move(reg_data);

  PostSuccess(std::move(success_callback));
}

void BluetoothSocketWin::DoAccept(AcceptCompletionCallback success_callback,
                                  ErrorCompletionCallback error_callback) {
  DCHECK(socket_thread()->task_runner()->RunsTasksInCurrentSequence());
  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));
  int result = tcp_socket()->Accept(
      &accept_socket_, &accept_address_,
      base::BindOnce(&BluetoothSocketWin::OnAcceptOnSocketThread, this,
                     std::move(success_callback),
                     std::move(split_error_callback.first)));
  if (result != net::OK && result != net::ERR_IO_PENDING) {
    LOG(WARNING) << "Failed to accept, net err=" << result;
    PostErrorCompletion(std::move(split_error_callback.second),
                        kFailedToAccept);
  }
}

void BluetoothSocketWin::OnAcceptOnSocketThread(
    AcceptCompletionCallback success_callback,
    ErrorCompletionCallback error_callback,
    int accept_result) {
  DCHECK(socket_thread()->task_runner()->RunsTasksInCurrentSequence());
  if (accept_result != net::OK) {
    LOG(WARNING) << "OnAccept error, net err=" << accept_result;
    PostErrorCompletion(std::move(error_callback), kFailedToAccept);
    return;
  }

  ui_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSocketWin::OnAcceptOnUI, this,
                     std::move(accept_socket_), accept_address_,
                     std::move(success_callback), std::move(error_callback)));
}

void BluetoothSocketWin::OnAcceptOnUI(
    std::unique_ptr<net::TCPSocket> accept_socket,
    const net::IPEndPoint& peer_address,
    AcceptCompletionCallback success_callback,
    ErrorCompletionCallback error_callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  const std::string peer_device_address =
      IPEndPointToBluetoothAddress(peer_address);
  const BluetoothDevice* peer_device = adapter_->GetDevice(peer_device_address);
  if (!peer_device) {
    LOG(WARNING) << "OnAccept failed with unknown device, addr="
                 << peer_device_address;
    std::move(error_callback).Run(kFailedToAccept);
    return;
  }

  scoped_refptr<BluetoothSocketWin> peer_socket =
      CreateBluetoothSocket(ui_task_runner(), socket_thread());
  peer_socket->SetTCPSocket(std::move(accept_socket));
  std::move(success_callback).Run(peer_device, peer_socket);
}

}  // namespace device
