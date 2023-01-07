// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/private/net_address_private_impl.h"

#include "build/build_config.h"
#include "ppapi/c/private/ppb_net_address_private.h"

namespace ppapi {

#if !BUILDFLAG(IS_NACL)
// static
const PP_NetAddress_Private NetAddressPrivateImpl::kInvalidNetAddress = { 0 };
#endif  // !BUILDFLAG(IS_NACL)

}  // namespace ppapi
