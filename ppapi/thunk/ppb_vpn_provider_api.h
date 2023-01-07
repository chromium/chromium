// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_VPN_PROVIDER_API_H_
#define PPAPI_THUNK_PPB_VPN_PROVIDER_API_H_

#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_VpnProvider_API {
 public:
  virtual ~PPB_VpnProvider_API() {}

  virtual int32_t Bind(const PP_Var& configuration_id,
                       const PP_Var& configuration_name,
                       const scoped_refptr<TrackedCallback>& callback) = 0;
  virtual int32_t SendPacket(
      const PP_Var& packet,
      const scoped_refptr<TrackedCallback>& callback) = 0;
  virtual int32_t ReceivePacket(
      PP_Var* packet,
      const scoped_refptr<TrackedCallback>& callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_VPN_PROVIDER_API_H_
