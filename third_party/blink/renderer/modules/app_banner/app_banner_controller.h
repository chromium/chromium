// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_APP_BANNER_APP_BANNER_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_APP_BANNER_APP_BANNER_CONTROLLER_H_

#include "third_party/blink/public/mojom/app_banner/app_banner.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LocalFrame;

class MODULES_EXPORT AppBannerController final
    : public mojom::blink::AppBannerController {
 public:
  explicit AppBannerController(LocalFrame&);

  static void BindMojoRequest(
      LocalFrame*,
      mojo::PendingReceiver<mojom::blink::AppBannerController>);

  void BannerPromptRequest(mojo::PendingRemote<mojom::blink::AppBannerService>,
                           mojo::PendingReceiver<mojom::blink::AppBannerEvent>,
                           const Vector<String>& platforms,
                           BannerPromptRequestCallback) override;

 private:
  WeakPersistent<LocalFrame> frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_APP_BANNER_APP_BANNER_CONTROLLER_H_
