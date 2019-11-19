// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_UTIL_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_UTIL_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace content {

CONTENT_EXPORT bool AreScanFiltersSame(
    const blink::mojom::WebBluetoothLeScanFilter& filter_1,
    const blink::mojom::WebBluetoothLeScanFilter& filter_2);

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_UTIL_H_
