// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEFS_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEFS_WIN_H_

#include <windows.h>
#include <cfg.h>
#include <devpkey.h>
#include <ntverp.h>  // For VER_PRODUCTBUILD
#include <setupapi.h>

#if VER_PRODUCTBUILD > 9600
// bthledef.h is fixed in the Windows 10 SDK and the extra pop then triggers a
// warning, so we skip it when VER_PRODUCTBUILD is > 9600 (8.1 SDK)
#include <bthledef.h>
#else
#pragma warning(push)
// bthledef.h in the Windows 8.1 SDK is buggy and contains
//   #pragma pop
// which should be
//   #pragma warning(pop)
// So, we disable the "unknown pragma" warning, then actually pop, and then pop
// our disabling of 4068.
#pragma warning(disable: 4068)
#include <bthledef.h>
#pragma warning(pop)
#pragma warning(pop)
#endif

#include <bluetoothapis.h>
#include <bluetoothleapis.h>

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEFS_WIN_H_
