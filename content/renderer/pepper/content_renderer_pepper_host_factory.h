// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_CONTENT_RENDERER_PEPPER_HOST_FACTORY_H_
#define CONTENT_RENDERER_PEPPER_CONTENT_RENDERER_PEPPER_HOST_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "ppapi/host/host_factory.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

namespace ppapi {
class PpapiPermissions;
}

namespace content {
class RendererPpapiHostImpl;

class ContentRendererPepperHostFactory : public ppapi::host::HostFactory {
 public:
  explicit ContentRendererPepperHostFactory(RendererPpapiHostImpl* host);

  ContentRendererPepperHostFactory(const ContentRendererPepperHostFactory&) =
      delete;
  ContentRendererPepperHostFactory& operator=(
      const ContentRendererPepperHostFactory&) = delete;

  ~ContentRendererPepperHostFactory() override;

  std::unique_ptr<ppapi::host::ResourceHost> CreateResourceHost(
      ppapi::host::PpapiHost* host,
      PP_Resource resource,
      PP_Instance instance,
      const IPC::Message& message) override;

 private:
  const ppapi::PpapiPermissions& GetPermissions() const;

  // Non-owning pointer.
  raw_ptr<RendererPpapiHostImpl> host_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_CONTENT_RENDERER_PEPPER_HOST_FACTORY_H_
