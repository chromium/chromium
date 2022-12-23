// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_
#define CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "content/common/content_export.h"
#include "content/public/browser/smart_card_delegate.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom.h"

namespace content {

class RenderFrameHostImpl;

// SmarCardService provides an implementation of the SmartCardService mojom
// interface. This interface is used by Blink to implement the Web Smart Card
// API.
class CONTENT_EXPORT SmartCardService : public blink::mojom::SmartCardService,
                                        public SmartCardDelegate::Observer {
 public:
  explicit SmartCardService(SmartCardDelegate& delegate);
  ~SmartCardService() override;

  // Use this when creating from a document.
  static void Create(RenderFrameHostImpl*,
                     mojo::PendingReceiver<blink::mojom::SmartCardService>);

  // blink::mojom::SmartCardService overrides:
  void GetReaders(GetReadersCallback callback) override;
  void RegisterClient(mojo::PendingAssociatedRemote<
                          device::mojom::SmartCardManagerClient> client,
                      RegisterClientCallback callback) override;

  // SmartCardDelegate::Observer overrides:
  void OnReaderAdded(
      const device::mojom::SmartCardReaderInfo& reader_info) override;
  void OnReaderRemoved(
      const device::mojom::SmartCardReaderInfo& reader_info) override;
  void OnReaderChanged(
      const device::mojom::SmartCardReaderInfo& reader_info) override;

 private:
  const raw_ref<SmartCardDelegate> delegate_;
  base::ScopedObservation<SmartCardDelegate, SmartCardDelegate::Observer>
      scoped_observation_{this};

  // Used to bind with Blink.
  mojo::AssociatedRemoteSet<device::mojom::SmartCardManagerClient> clients_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_
