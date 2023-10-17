// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SMART_CARD_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SMART_CARD_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/smart_card.mojom-forward.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom-forward.h"

namespace content {
class BrowserContext;

// Interface provided by the content embedder to support the Web Smart Card
// API.
class CONTENT_EXPORT SmartCardDelegate {
 public:
  SmartCardDelegate() = default;
  SmartCardDelegate(SmartCardDelegate&) = delete;
  SmartCardDelegate& operator=(SmartCardDelegate&) = delete;
  virtual ~SmartCardDelegate() = default;

  virtual mojo::PendingRemote<device::mojom::SmartCardContextFactory>
  GetSmartCardContextFactory(BrowserContext& browser_context) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SMART_CARD_DELEGATE_H_
