// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_SERIALIZED_STRUCTS_H_
#define PPAPI_PROXY_SERIALIZED_STRUCTS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "build/build_config.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/ppb_network_list.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/host_resource.h"

struct PP_BrowserFont_Trusted_Description;

namespace ppapi {
namespace proxy {

// PP_BrowserFontDescription  has to be redefined with a string in place of the
// PP_Var used for the face name.
struct PPAPI_PROXY_EXPORT SerializedFontDescription {
  SerializedFontDescription();
  ~SerializedFontDescription();

  void SetFromPPBrowserFontDescription(
      const PP_BrowserFont_Trusted_Description& desc);

  void SetToPPBrowserFontDescription(
      PP_BrowserFont_Trusted_Description* desc) const;

  std::string face;
  int32_t family;
  uint32_t size;
  int32_t weight;
  PP_Bool italic;
  PP_Bool small_caps;
  int32_t letter_spacing;
  int32_t word_spacing;
};

struct PPAPI_PROXY_EXPORT SerializedNetworkInfo {
  SerializedNetworkInfo();
  ~SerializedNetworkInfo();

  std::string name;
  PP_NetworkList_Type type;
  PP_NetworkList_State state;
  std::vector<PP_NetAddress_Private> addresses;
  std::string display_name;
  int mtu;
};
typedef std::vector<SerializedNetworkInfo> SerializedNetworkList;

struct SerializedDirEntry {
  std::string name;
  bool is_dir;
};

struct PPBURLLoader_UpdateProgress_Params {
  PP_Instance instance;
  ppapi::HostResource resource;
  int64_t bytes_sent;
  int64_t total_bytes_to_be_sent;
  int64_t bytes_received;
  int64_t total_bytes_to_be_received;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_SERIALIZED_STRUCTS_H_
