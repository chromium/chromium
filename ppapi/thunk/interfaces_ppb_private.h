// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

// no-include-guard-because-multiply-included

#include "build/build_config.h"
#include "ppapi/thunk/interfaces_preamble.h"

// See interfaces_ppb_private_no_permissions.h for other private interfaces.

PROXIED_API(PPB_X509Certificate_Private)

#if !BUILDFLAG(IS_NACL)
PROXIED_IFACE(PPB_X509CERTIFICATE_PRIVATE_INTERFACE_0_1,
              PPB_X509Certificate_Private_0_1)
PROXIED_IFACE(PPB_BROWSERFONT_TRUSTED_INTERFACE_1_0,
              PPB_BrowserFont_Trusted_1_0)
PROXIED_IFACE(PPB_CHARSET_TRUSTED_INTERFACE_1_0,
              PPB_CharSet_Trusted_1_0)
PROXIED_IFACE(PPB_FILECHOOSER_TRUSTED_INTERFACE_0_5,
              PPB_FileChooserTrusted_0_5)
PROXIED_IFACE(PPB_FILECHOOSER_TRUSTED_INTERFACE_0_6,
              PPB_FileChooserTrusted_0_6)
PROXIED_IFACE(PPB_FILEREFPRIVATE_INTERFACE_0_1,
              PPB_FileRefPrivate_0_1)
#endif  // !BUILDFLAG(IS_NACL)

#include "ppapi/thunk/interfaces_postamble.h"
