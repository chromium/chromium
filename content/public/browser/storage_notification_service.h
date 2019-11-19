// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STORAGE_NOTIFICATION_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_STORAGE_NOTIFICATION_SERVICE_H_

#include "base/bind.h"

namespace content {

// This interface is used to create a connection between the storage layer and
// the embedder layer, where calls to UI code can be made. Embedders should
// implement this interface as well as a GetStorageNotificationService()
// function in it's implementation of BrowserContext.
class StorageNotificationService {
 public:
  StorageNotificationService() = default;
  ~StorageNotificationService() = default;

  // This pure virtual function should be implemented in the embedder layer
  // where calls to UI and notification code can be implemented. This closure
  // is passed to QuotaManager in StoragePartitionImpl, where it is called
  // when QuotaManager determines appropriate to alert the user that the device
  // is in a state of storage pressure.
  virtual base::RepeatingClosure GetStoragePressureNotificationClosure() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(StorageNotificationService);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STORAGE_NOTIFICATION_SERVICE_H_
