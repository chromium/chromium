// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_NFC_NFC_UTILS_H_
#define SERVICES_DEVICE_PUBLIC_CPP_NFC_NFC_UTILS_H_

#include "base/component_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_IOS)
#include <CoreNFC/CoreNFC.h>

#include "services/device/public/mojom/nfc.mojom.h"
#endif

namespace device {

#if BUILDFLAG(IS_IOS)
// Maps a CoreNFC format to mojom raw format.
COMPONENT_EXPORT(NFC)
device::mojom::NSRawTypeNameFormat MapCoreNFCFormat(NFCTypeNameFormat format);
#endif

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_NFC_NFC_UTILS_H_
