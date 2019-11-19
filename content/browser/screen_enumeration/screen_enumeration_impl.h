// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_ENUMERATION_SCREEN_ENUMERATION_IMPL_H_
#define CONTENT_BROWSER_SCREEN_ENUMERATION_SCREEN_ENUMERATION_IMPL_H_

#include "third_party/blink/public/mojom/screen_enumeration/screen_enumeration.mojom.h"

namespace content {

// A backend for the proposed interface to query the device's screen space.
class ScreenEnumerationImpl : public blink::mojom::ScreenEnumeration {
 public:
  static void Create(
      mojo::PendingReceiver<blink::mojom::ScreenEnumeration> receiver);

  ScreenEnumerationImpl();
  ~ScreenEnumerationImpl() override;

  ScreenEnumerationImpl(const ScreenEnumerationImpl&) = delete;
  ScreenEnumerationImpl& operator=(const ScreenEnumerationImpl&) = delete;

  // blink::mojom::ScreenEnumeration:
  void GetDisplays(GetDisplaysCallback callback) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCREEN_ENUMERATION_SCREEN_ENUMERATION_IMPL_H_