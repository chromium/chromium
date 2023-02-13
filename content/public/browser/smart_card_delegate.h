// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SMART_CARD_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SMART_CARD_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom-forward.h"

namespace content {

// Interface provided by the content embedder to support the Web Smart Card
// API.
class CONTENT_EXPORT SmartCardDelegate {
 public:
  using GetReadersCallback =
      base::OnceCallback<void(blink::mojom::SmartCardGetReadersResultPtr)>;

  // Observer class for changes to smart card readers.
  //
  // SmartCardDelegate implementations are expected to call the observer
  // methods appropriately when a smart card reader is added, removed or
  // changed. The SmartCardDelegate the base class just takes care of
  // maintaining the observer_list_.
  class Observer : public base::CheckedObserver {
   public:
    // Called when a smart card reader is added to the system.
    // Depends on SupportsReaderAddedRemovedNotifications()
    // being true.
    virtual void OnReaderAdded(
        const blink::mojom::SmartCardReaderInfo& reader_info) = 0;

    // Called when a smart card reader is removed from the system.
    // Depends on SupportsReaderAddedRemovedNotifications()
    // being true.
    virtual void OnReaderRemoved(
        const blink::mojom::SmartCardReaderInfo& reader_info) = 0;

    // Called when the attributes (state and/or atr) of a smart card reader
    // changes.
    virtual void OnReaderChanged(
        const blink::mojom::SmartCardReaderInfo& reader_info) = 0;
  };

  SmartCardDelegate();
  SmartCardDelegate(SmartCardDelegate&) = delete;
  SmartCardDelegate& operator=(SmartCardDelegate&) = delete;
  virtual ~SmartCardDelegate();

  // Returns the list of smart card readers currently connected to the system.
  virtual void GetReaders(GetReadersCallback) = 0;

  // Whether the implementation supports notifying when a smart card
  // reader device is added or removed from the system.
  // Platform dependent.
  virtual bool SupportsReaderAddedRemovedNotifications() const = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SMART_CARD_DELEGATE_H_
