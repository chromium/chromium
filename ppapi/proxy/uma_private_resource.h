// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_UMA_PRIVATE_RESOURCE_H_
#define PPAPI_PROXY_UMA_PRIVATE_RESOURCE_H_

#include <stdint.h>

#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_uma_singleton_api.h"

namespace ppapi {

namespace proxy {

class PPAPI_PROXY_EXPORT UMAPrivateResource
    : public PluginResource,
      public thunk::PPB_UMA_Singleton_API {
 public:
  UMAPrivateResource(Connection connection, PP_Instance instance);

  UMAPrivateResource(const UMAPrivateResource&) = delete;
  UMAPrivateResource& operator=(const UMAPrivateResource&) = delete;

  ~UMAPrivateResource() override;

  // Resource overrides.
  thunk::PPB_UMA_Singleton_API* AsPPB_UMA_Singleton_API() override;

  // PPB_UMA_Singleton_API implementation.
  void HistogramCustomTimes(PP_Instance instance,
                            struct PP_Var name,
                            int64_t sample,
                            int64_t min,
                            int64_t max,
                            uint32_t bucket_count) override;

  void HistogramCustomCounts(PP_Instance instance,
                             struct PP_Var name,
                             int32_t sample,
                             int32_t min,
                             int32_t max,
                             uint32_t bucket_count) override;

  void HistogramEnumeration(PP_Instance instance,
                            struct PP_Var name,
                            int32_t sample,
                            int32_t boundary_value) override;

  int32_t IsCrashReportingEnabled(
      PP_Instance instance,
      scoped_refptr<TrackedCallback> callback) override;

 private:
  void OnPluginMsgIsCrashReportingEnabled(
      const ResourceMessageReplyParams& params);
  scoped_refptr<TrackedCallback> pending_callback_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_UMA_PRIVATE_RESOURCE_H_
