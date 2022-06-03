// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions/permission_utils.h"

#include "base/notreached.h"

namespace blink {

mojom::PermissionStatus ToPermissionStatus(const std::string& status) {
  if (status == "granted")
    return mojom::PermissionStatus::GRANTED;
  if (status == "prompt")
    return mojom::PermissionStatus::ASK;
  if (status == "denied")
    return mojom::PermissionStatus::DENIED;
  NOTREACHED();
  return mojom::PermissionStatus::DENIED;
}

}  // namespace blink
