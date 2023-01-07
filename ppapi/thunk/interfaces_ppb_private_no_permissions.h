// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

#include "ppapi/thunk/interfaces_preamble.h"

// These interfaces don't require private permissions. However, they only work
// for whitelisted origins.

PROXIED_IFACE(PPB_CAMERACAPABILITIES_PRIVATE_INTERFACE_0_1,
              PPB_CameraCapabilities_Private_0_1)
PROXIED_IFACE(PPB_CAMERADEVICE_PRIVATE_INTERFACE_0_1,
              PPB_CameraDevice_Private_0_1)

PROXIED_IFACE(PPB_HOSTRESOLVER_PRIVATE_INTERFACE_0_1,
              PPB_HostResolver_Private_0_1)

PROXIED_IFACE(PPB_NETADDRESS_PRIVATE_INTERFACE_0_1,
              PPB_NetAddress_Private_0_1)
PROXIED_IFACE(PPB_NETADDRESS_PRIVATE_INTERFACE_1_0,
              PPB_NetAddress_Private_1_0)
PROXIED_IFACE(PPB_NETADDRESS_PRIVATE_INTERFACE_1_1,
              PPB_NetAddress_Private_1_1)

PROXIED_IFACE(PPB_EXT_CRXFILESYSTEM_PRIVATE_INTERFACE_0_1,
              PPB_Ext_CrxFileSystem_Private_0_1)
PROXIED_IFACE(PPB_FILEIO_PRIVATE_INTERFACE_0_1,
              PPB_FileIO_Private_0_1)
PROXIED_IFACE(PPB_ISOLATEDFILESYSTEM_PRIVATE_INTERFACE_0_2,
              PPB_IsolatedFileSystem_Private_0_2)

PROXIED_IFACE(PPB_UMA_PRIVATE_INTERFACE_0_3,
              PPB_UMA_Private_0_3)

// This has permission checks done in pepper_url_loader_host.cc
PROXIED_IFACE(PPB_URLLOADERTRUSTED_INTERFACE_0_3,
              PPB_URLLoaderTrusted_0_3)

#include "ppapi/thunk/interfaces_postamble.h"
