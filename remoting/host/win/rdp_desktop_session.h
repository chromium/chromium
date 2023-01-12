// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_RDP_DESKTOP_SESSION_H_
#define REMOTING_HOST_WIN_RDP_DESKTOP_SESSION_H_

#include <wrl/client.h>

#include <memory>

#include "base/win/atl.h"
// chromoting_lib.h contains MIDL-generated declarations.
#include "remoting/host/win/chromoting_lib.h"
#include "remoting/host/win/rdp_client.h"

namespace remoting {

// Implements IRdpDesktopSession interface providing a way to host RdpClient
// objects in a COM component.
class __declspec(uuid(RDP_DESKTOP_SESSION_CLSID)) RdpDesktopSession
    : public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>,
      public ATL::CComCoClass<RdpDesktopSession, &__uuidof(RdpDesktopSession)>,
      public IRdpDesktopSession,
      public RdpClient::EventHandler {
 public:
  // Declare a class factory which must not lock the ATL module. This is the
  // same as DECLARE_CLASSFACTORY() with the exception that
  // ATL::CComObjectNoLock is used unconditionally.
  //
  // By default ATL generates locking class factories (by wrapping them in
  // ATL::CComObjectCached) for classes hosted in a DLL. This class is compiled
  // into a DLL but it is registered as an out-of-process class, so its class
  // factory should not use locking.
  typedef ATL::CComCreator<ATL::CComObjectNoLock<ATL::CComClassFactory>>
      _ClassFactoryCreatorClass;

  RdpDesktopSession();
  ~RdpDesktopSession() override;

  // IRdpDesktopSession implementation.
  STDMETHOD(Connect)
  (long width,
   long height,
   long dpi_x,
   long dpi_y,
   BSTR terminal_id,
   DWORD port_number,
   IRdpDesktopSessionEventHandler* event_handler) override;
  STDMETHOD(Disconnect)() override;
  STDMETHOD(ChangeResolution)
  (long width, long height, long dpi_x, long dpi_y) override;
  STDMETHOD(InjectSas)() override;

  DECLARE_NO_REGISTRY()

 private:
  // RdpClient::EventHandler interface.
  void OnRdpConnected() override;
  void OnRdpClosed() override;

  BEGIN_COM_MAP(RdpDesktopSession)
  COM_INTERFACE_ENTRY(IRdpDesktopSession)
  COM_INTERFACE_ENTRY(IUnknown)
  END_COM_MAP()

  // Implements loading and instantiation of the RDP ActiveX client.
  std::unique_ptr<RdpClient> client_;

  // Holds a reference to the caller's EventHandler, through which notifications
  // are dispatched. Released in Disconnect(), to prevent further notifications.
  Microsoft::WRL::ComPtr<IRdpDesktopSessionEventHandler> event_handler_;

  DECLARE_PROTECT_FINAL_CONSTRUCT()
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_RDP_DESKTOP_SESSION_H_
