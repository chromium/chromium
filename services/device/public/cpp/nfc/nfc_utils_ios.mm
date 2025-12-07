// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/nfc/nfc_utils.h"

#include "base/notreached.h"

namespace device {

#if BUILDFLAG(IS_IOS)
device::mojom::NSRawTypeNameFormat MapCoreNFCFormat(NFCTypeNameFormat format) {
  switch (format) {
    case NFCTypeNameFormatEmpty:
      return device::mojom::NSRawTypeNameFormat::kEmpty;
    case NFCTypeNameFormatAbsoluteURI:
      return device::mojom::NSRawTypeNameFormat::kAbsoluteURI;
    case NFCTypeNameFormatMedia:
      return device::mojom::NSRawTypeNameFormat::kMedia;
    case NFCTypeNameFormatNFCExternal:
      return device::mojom::NSRawTypeNameFormat::kExternal;
    case NFCTypeNameFormatNFCWellKnown:
      return device::mojom::NSRawTypeNameFormat::kWellKnown;
    case NFCTypeNameFormatUnchanged:
      return device::mojom::NSRawTypeNameFormat::kUnchanged;
    case NFCTypeNameFormatUnknown:
      return device::mojom::NSRawTypeNameFormat::kUnknown;
  }
  NOTREACHED();
}
#endif

}  // namespace device
