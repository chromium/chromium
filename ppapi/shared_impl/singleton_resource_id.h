// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_SINGLETON_RESOURCE_ID_H_
#define PPAPI_SHARED_IMPL_SINGLETON_RESOURCE_ID_H_

namespace ppapi {

// These IDs are used to access singleton resource objects using
// PPB_Instance_API.GetSingletonResource.
enum SingletonResourceID {
  BROWSER_FONT_SINGLETON_ID,
  GAMEPAD_SINGLETON_ID,
  ISOLATED_FILESYSTEM_SINGLETON_ID,
  NETWORK_PROXY_SINGLETON_ID,
  UMA_SINGLETON_ID,
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_SINGLETON_RESOURCE_ID_H_
