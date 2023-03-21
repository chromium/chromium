// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_
#define CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_

#include "base/memory/raw_ref.h"
#include "content/browser/smart_card/smart_card_reader_tracker.h"
#include "content/common/content_export.h"
#include "content/public/browser/smart_card_delegate.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom.h"

namespace content {

class RenderFrameHostImpl;

// SmarCardService provides an implementation of the SmartCardService mojom
// interface. This interface is used by Blink to implement the Web Smart Card
// API.
class CONTENT_EXPORT SmartCardService
    : public blink::mojom::SmartCardService,
      public SmartCardReaderTracker::Observer {
 public:
  explicit SmartCardService(SmartCardDelegate& delegate,
                            SmartCardReaderTracker& reader_tracker);
  ~SmartCardService() override;

  // Use this when creating from a document.
  static void Create(RenderFrameHostImpl*,
                     mojo::PendingReceiver<blink::mojom::SmartCardService>);

  // blink::mojom::SmartCardService overrides:
  void GetReadersAndStartTracking(
      GetReadersAndStartTrackingCallback callback) override;
  void RegisterClient(mojo::PendingAssociatedRemote<
                          blink::mojom::SmartCardServiceClient> client,
                      RegisterClientCallback callback) override;

  // SmartCardReaderTracker::Observer overrides:
  void OnReaderAdded(
      const blink::mojom::SmartCardReaderInfo& reader_info) override;
  void OnReaderRemoved(
      const blink::mojom::SmartCardReaderInfo& reader_info) override;
  void OnReaderChanged(
      const blink::mojom::SmartCardReaderInfo& reader_info) override;
  void OnError(blink::mojom::SmartCardResponseCode response_code) override;

 private:
  const raw_ref<SmartCardDelegate> delegate_;
  const raw_ref<SmartCardReaderTracker> reader_tracker_;

  // Used to bind with Blink.
  mojo::AssociatedRemoteSet<blink::mojom::SmartCardServiceClient> clients_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMART_CARD_SMART_CARD_SERVICE_H_
