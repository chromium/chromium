// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_HOST_RESOLVER_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_HOST_RESOLVER_PRIVATE_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

struct PP_NetAddress_Private;

namespace pp {

class CompletionCallback;
class InstanceHandle;

class HostResolverPrivate : public Resource {
 public:
  explicit HostResolverPrivate(const InstanceHandle& instance);

  // Returns true if the required interface is available.
  static bool IsAvailable();

  int32_t Resolve(const std::string& host,
                  uint16_t port,
                  const PP_HostResolver_Private_Hint& hint,
                  const CompletionCallback& callback);
  Var GetCanonicalName();
  uint32_t GetSize();
  bool GetNetAddress(uint32_t index, PP_NetAddress_Private* address);
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_HOST_RESOLVER_PRIVATE_H_
