// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/browser_font_singleton_resource.h"

#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

BrowserFontSingletonResource::BrowserFontSingletonResource(
    Connection connection,
    PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(BROWSER, PpapiHostMsg_BrowserFontSingleton_Create());
}

BrowserFontSingletonResource::~BrowserFontSingletonResource() {
}

thunk::PPB_BrowserFont_Singleton_API*
BrowserFontSingletonResource::AsPPB_BrowserFont_Singleton_API() {
  return this;
}

PP_Var BrowserFontSingletonResource::GetFontFamilies(PP_Instance instance) {
  if (families_.empty()) {
    SyncCall<PpapiPluginMsg_BrowserFontSingleton_GetFontFamiliesReply>(
        BROWSER, PpapiHostMsg_BrowserFontSingleton_GetFontFamilies(),
        &families_);
  }
  return StringVar::StringToPPVar(families_);
}

}  // namespace proxy
}  // namespace ppapi
