// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_
#define CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_

#include "base/containers/queue.h"
#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/smart_card.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom.h"

namespace content {

class RenderFrameHostImpl;

// SmarCardService provides an implementation of the SmartCardService mojom
// interface. This interface is used by Blink to implement the Web Smart Card
// API.
class CONTENT_EXPORT SmartCardService : public blink::mojom::SmartCardService {
 public:
  explicit SmartCardService(
      mojo::PendingRemote<device::mojom::SmartCardContextFactory>
          context_factory);
  ~SmartCardService() override;

  // Use this when creating from a document.
  static void Create(RenderFrameHostImpl*,
                     mojo::PendingReceiver<blink::mojom::SmartCardService>);

  // blink::mojom::SmartCardService overrides:
  void CreateContext(CreateContextCallback callback) override;

 private:
  mojo::Remote<device::mojom::SmartCardContextFactory> context_factory_;
  base::WeakPtrFactory<SmartCardService> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_
