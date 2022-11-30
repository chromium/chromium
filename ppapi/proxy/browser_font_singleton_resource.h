// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_BROWSER_FONT_SINGLETON_RESOURCE_H_
#define PPAPI_PROXY_BROWSER_FONT_SINGLETON_RESOURCE_H_

#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/thunk/ppb_browser_font_singleton_api.h"

namespace ppapi {
namespace proxy {

// This handles the singleton calls (that don't take a PP_Resource parameter)
// on the browser font interface
class BrowserFontSingletonResource
    : public PluginResource,
      public thunk::PPB_BrowserFont_Singleton_API {
 public:
  BrowserFontSingletonResource(Connection connection, PP_Instance instance);

  BrowserFontSingletonResource(const BrowserFontSingletonResource&) = delete;
  BrowserFontSingletonResource& operator=(const BrowserFontSingletonResource&) =
      delete;

  ~BrowserFontSingletonResource() override;

  // Resource override.
  thunk::PPB_BrowserFont_Singleton_API*
      AsPPB_BrowserFont_Singleton_API() override;

  // thunk::PPB_BrowserFontSingleton_API implementation.
  PP_Var GetFontFamilies(PP_Instance instance) override;

 private:
  // Lazily-filled-in list of font families.
  std::string families_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_BROWSER_FONT_SINGLETON_RESOURCE_H_
