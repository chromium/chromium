// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/udev_linux/udev_loader.h"

#include <memory>

#include "base/check.h"
#include "device/udev_linux/udev0_loader.h"
#include "device/udev_linux/udev1_loader.h"

namespace device {

namespace {

UdevLoader* g_udev_loader = NULL;

}  // namespace

// static
UdevLoader* UdevLoader::Get() {
  if (g_udev_loader)
    return g_udev_loader;

  std::unique_ptr<UdevLoader> udev_loader = std::make_unique<Udev1Loader>();
  if (udev_loader->Init()) {
    g_udev_loader = udev_loader.release();
    return g_udev_loader;
  }

  udev_loader = std::make_unique<Udev0Loader>();
  if (udev_loader->Init()) {
    g_udev_loader = udev_loader.release();
    return g_udev_loader;
  }
  CHECK(false);
  return nullptr;
}

// static
void UdevLoader::SetForTesting(UdevLoader* loader, bool delete_previous) {
  if (g_udev_loader && delete_previous)
    delete g_udev_loader;

  g_udev_loader = loader;
}

UdevLoader::~UdevLoader() = default;

}  // namespace device
