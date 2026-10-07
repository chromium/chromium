// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_RDP_DESKTOP_SESSION_H_
#define REMOTING_HOST_WIN_RDP_DESKTOP_SESSION_H_

#include <wrl/client.h>
#include <wrl/implements.h>

#include <memory>

// chromoting_lib.h contains MIDL-generated declarations.
#include "remoting/host/win/chromoting_lib.h"
#include "remoting/host/win/rdp_client.h"

namespace remoting {

// Implements IRdpDesktopSession interface providing a way to host RdpClient
// objects in a COM component.
class __declspec(uuid(RDP_DESKTOP_SESSION_CLSID)) RdpDesktopSession
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IRdpDesktopSession>,
      public RdpClient::EventHandler {
 public:
  RdpDesktopSession();

  RdpDesktopSession(const RdpDesktopSession&) = delete;
  RdpDesktopSession& operator=(const RdpDesktopSession&) = delete;

  ~RdpDesktopSession() override;

  // IRdpDesktopSession implementation.
  IFACEMETHODIMP Connect(
      long width,
      long height,
      long dpi_x,
      long dpi_y,
      BSTR terminal_id,
      DWORD port_number,
      IRdpDesktopSessionEventHandler* event_handler) override;
  IFACEMETHODIMP Disconnect() override;
  IFACEMETHODIMP ChangeResolution(long width,
                                  long height,
                                  long dpi_x,
                                  long dpi_y) override;
  IFACEMETHODIMP InjectSas() override;

 private:
  // RdpClient::EventHandler interface.
  void OnRdpConnected() override;
  void OnRdpClosed() override;

  // Implements loading and instantiation of the RDP ActiveX client.
  std::unique_ptr<RdpClient> client_;

  // Holds a reference to the caller's EventHandler, through which notifications
  // are dispatched. Released in Disconnect(), to prevent further notifications.
  Microsoft::WRL::ComPtr<IRdpDesktopSessionEventHandler> event_handler_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_RDP_DESKTOP_SESSION_H_
