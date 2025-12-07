// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/udev_linux/udev_loader.h"

#include <memory>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "device/udev_linux/udev1_loader.h"

namespace device {

namespace {

UdevLoader* g_udev_loader = nullptr;

// Provides a lock to synchronize initializing and accessing `g_udev_loader`
// across threads.
base::Lock& GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

}  // namespace

// static
UdevLoader* UdevLoader::Get() {
  base::AutoLock guard(GetLock());

  if (!g_udev_loader) {
    g_udev_loader = new Udev1Loader();
  }
  return g_udev_loader;
}

// static
void UdevLoader::SetForTesting(UdevLoader* loader, bool delete_previous) {
  base::AutoLock guard(GetLock());

  if (g_udev_loader && delete_previous)
    delete g_udev_loader;

  g_udev_loader = loader;
}

UdevLoader::~UdevLoader() = default;

}  // namespace device
