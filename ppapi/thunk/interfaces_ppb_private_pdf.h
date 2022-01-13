// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

// no-include-guard-because-multiply-included

#include "build/build_config.h"
#include "ppapi/thunk/interfaces_preamble.h"

// See interfaces_ppb_private_no_permissions.h for other private interfaces.

#if !BUILDFLAG(IS_NACL)
PROXIED_IFACE(PPB_PDF_INTERFACE, PPB_PDF)
PROXIED_IFACE(PPB_FIND_PRIVATE_INTERFACE_0_3, PPB_Find_Private_0_3)
#endif  // !BUILDFLAG(IS_NACL)

#include "ppapi/thunk/interfaces_postamble.h"
