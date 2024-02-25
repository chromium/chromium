// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_IN_PROCESS_RESOURCE_CREATION_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_IN_PROCESS_RESOURCE_CREATION_H_

#include "base/memory/raw_ptr.h"
#include "content/renderer/pepper/resource_creation_impl.h"
#include "ppapi/proxy/connection.h"

namespace content {

class RendererPpapiHostImpl;

// This class provides creation functions for the new resources with IPC
// backends that live in content/renderer/pepper.
//
// (See pepper_in_process_router.h for more information.)
//
// This is a bit confusing. The "old-style" resources live in
// content/renderer/pepper and are created by the ResourceCreationImpl in that
// directory. The "new-style" IPC-only resources are in ppapi/proxy and are
// created by the RessourceCreationProxy in that directory.
//
// This class allows us to run new-style IPC-only resources in-process. We use
// the PepperInProcessRouter to run it in process. But then we have a problem
// with allocating the resources since src/webkit can't depend on IPC or see
// our IPC backend in content. This class overrides the normal in-process
// resource creation and adds in the resources that we implement in
// ppapi/proxy.
//
// When we convert all resources to use the new-style, we can just use the
// ResourceCreationProxy for all resources. This class is just glue to manage
// the temporary "two different cases."
class PepperInProcessResourceCreation : public ResourceCreationImpl {
 public:
  PepperInProcessResourceCreation(RendererPpapiHostImpl* host_impl,
                                  PepperPluginInstanceImpl* instance);

  PepperInProcessResourceCreation(const PepperInProcessResourceCreation&) =
      delete;
  PepperInProcessResourceCreation& operator=(
      const PepperInProcessResourceCreation&) = delete;

  ~PepperInProcessResourceCreation() override;

  // ResourceCreation_API implementation.
  PP_Resource CreateBrowserFont(
      PP_Instance instance,
      const PP_BrowserFont_Trusted_Description* description) override;
  PP_Resource CreateFileChooser(PP_Instance instance,
                                PP_FileChooserMode_Dev mode,
                                const PP_Var& accept_types) override;
  PP_Resource CreateFileIO(PP_Instance instance) override;
  PP_Resource CreateFileRef(
      PP_Instance instance,
      const ppapi::FileRefCreateInfo& create_info) override;
  PP_Resource CreateFileSystem(PP_Instance instance,
                               PP_FileSystemType type) override;
  PP_Resource CreateGraphics2D(PP_Instance pp_instance,
                               const PP_Size* size,
                               PP_Bool is_always_opaque) override;
  PP_Resource CreatePrinting(PP_Instance instance) override;
  PP_Resource CreateURLLoader(PP_Instance instance) override;
  PP_Resource CreateURLRequestInfo(PP_Instance instance) override;
  PP_Resource CreateWebSocket(PP_Instance instance) override;

 private:
  // Non-owning pointer to the host for the current plugin.
  raw_ptr<RendererPpapiHostImpl> host_impl_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_IN_PROCESS_RESOURCE_CREATION_H_
