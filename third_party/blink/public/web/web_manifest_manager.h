// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MANIFEST_MANAGER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MANIFEST_MANAGER_H_

#include "base/callback.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

struct Manifest;
class WebLocalFrame;
class WebURL;

class WebManifestManager {
 public:
  using Callback = base::OnceCallback<void(const WebURL&, const Manifest&)>;

  BLINK_EXPORT static void RequestManifestForTesting(WebLocalFrame*,
                                                     Callback callback);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MANIFEST_MANAGER_H_
