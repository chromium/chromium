// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INSTALLATION_INSTALLATION_SERVICE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INSTALLATION_INSTALLATION_SERVICE_IMPL_H_

#include "third_party/blink/public/mojom/installation/installation.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class LocalFrame;

class MODULES_EXPORT InstallationServiceImpl final
    : public mojom::blink::InstallationService {
 public:
  explicit InstallationServiceImpl(LocalFrame&);

  static void Create(
      LocalFrame*,
      mojo::PendingReceiver<mojom::blink::InstallationService> receiver);

  void OnInstall() override;

 private:
  WeakPersistent<LocalFrame> frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INSTALLATION_INSTALLATION_SERVICE_IMPL_H_
