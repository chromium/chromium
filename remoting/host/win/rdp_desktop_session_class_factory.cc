// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/rdp_desktop_session_class_factory.h"

#include <wrl/client.h>

#include "remoting/host/win/rdp_desktop_session.h"

namespace remoting {

RdpDesktopSessionClassFactory::RdpDesktopSessionClassFactory() = default;

RdpDesktopSessionClassFactory::~RdpDesktopSessionClassFactory() = default;

IFACEMETHODIMP RdpDesktopSessionClassFactory::CreateInstance(IUnknown* outer,
                                                             REFIID riid,
                                                             void** ppv) {
  if (outer) {
    return CLASS_E_NOAGGREGATION;
  }
  auto session = Microsoft::WRL::Make<RdpDesktopSession>();
  if (!session) {
    return E_OUTOFMEMORY;
  }
  return session.CopyTo(riid, ppv);
}

}  // namespace remoting
