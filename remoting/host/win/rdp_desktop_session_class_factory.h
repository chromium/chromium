// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_RDP_DESKTOP_SESSION_CLASS_FACTORY_H_
#define REMOTING_HOST_WIN_RDP_DESKTOP_SESSION_CLASS_FACTORY_H_

#include <wrl/client.h>

#include "base/win/wrl_module.h"

namespace remoting {

class RdpDesktopSessionClassFactory : public Microsoft::WRL::ClassFactory<> {
 public:
  RdpDesktopSessionClassFactory();

  RdpDesktopSessionClassFactory(const RdpDesktopSessionClassFactory&) = delete;
  RdpDesktopSessionClassFactory& operator=(
      const RdpDesktopSessionClassFactory&) = delete;

  ~RdpDesktopSessionClassFactory() override;

  // IClassFactory from Microsoft::WRL::ClassFactory:
  IFACEMETHODIMP CreateInstance(IUnknown* outer,
                                REFIID riid,
                                void** ppv) override;
};

using RdpDesktopSessionFactory = RdpDesktopSessionClassFactory;

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_RDP_DESKTOP_SESSION_CLASS_FACTORY_H_
