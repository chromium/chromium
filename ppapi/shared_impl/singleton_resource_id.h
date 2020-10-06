// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_SINGLETON_RESOURCE_ID_H_
#define PPAPI_SHARED_IMPL_SINGLETON_RESOURCE_ID_H_

namespace ppapi {

// These IDs are used to access singleton resource objects using
// PPB_Instance_API.GetSingletonResource.
enum SingletonResourceID {
  // TODO(raymes): The broker resource isn't really a singleton. This is only
  // a hack until PPB_Broker trusted has been fully refactored to the new
  // resource model.
  BROKER_SINGLETON_ID,
  BROWSER_FONT_SINGLETON_ID,
  FLASH_CLIPBOARD_SINGLETON_ID,
  FLASH_FILE_SINGLETON_ID,
  FLASH_FULLSCREEN_SINGLETON_ID,
  FLASH_SINGLETON_ID,
  GAMEPAD_SINGLETON_ID,
  ISOLATED_FILESYSTEM_SINGLETON_ID,
  NETWORK_PROXY_SINGLETON_ID,
  PDF_SINGLETON_ID,
  UMA_SINGLETON_ID,
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_SINGLETON_RESOURCE_ID_H_
