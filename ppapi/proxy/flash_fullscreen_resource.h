// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FLASH_FULLSCREEN_RESOURCE_H_
#define PPAPI_PROXY_FLASH_FULLSCREEN_RESOURCE_H_

#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/thunk/ppb_flash_fullscreen_api.h"

namespace ppapi {
namespace proxy {

class FlashFullscreenResource
    : public PluginResource,
      public thunk::PPB_Flash_Fullscreen_API {
 public:
  FlashFullscreenResource(Connection connection,
                          PP_Instance instance);

  FlashFullscreenResource(const FlashFullscreenResource&) = delete;
  FlashFullscreenResource& operator=(const FlashFullscreenResource&) = delete;

  ~FlashFullscreenResource() override;

  // Resource overrides.
  thunk::PPB_Flash_Fullscreen_API* AsPPB_Flash_Fullscreen_API() override;

  // PPB_Flash_Fullscreen_API implementation.
  PP_Bool IsFullscreen(PP_Instance instance) override;
  PP_Bool SetFullscreen(PP_Instance instance, PP_Bool fullscreen) override;
  void SetLocalIsFullscreen(PP_Instance instance,
                            PP_Bool is_fullscreen) override;

 private:
  PP_Bool is_fullscreen_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FLASH_FULLSCREEN_RESOURCE_H_
