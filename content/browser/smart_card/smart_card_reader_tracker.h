// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_H_
#define CONTENT_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_H_

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom.h"

namespace content {
class BrowserContext;
class SmartCardDelegate;

// Keeps track of the current list of readers and their states by querying the
// given SmartCardProvider.
//
// Translates the winscard.h level constructs involving reader state into the
// higher-level `SmartCardReaderInfo`.
class CONTENT_EXPORT SmartCardReaderTracker
    : public base::SupportsUserData::Data {
 public:
  // Observer class for changes to smart card readers.
  class Observer : public base::CheckedObserver {
   public:
    // Called when a smart card reader is added to the system.
    //
    // Depends on SmartCardDelegate::SupportsReaderAddedRemovedNotifications()
    // being true.
    virtual void OnReaderAdded(
        const blink::mojom::SmartCardReaderInfo& reader_info) = 0;

    // Called when a smart card reader is removed from the system.
    virtual void OnReaderRemoved(
        const blink::mojom::SmartCardReaderInfo& reader_info) = 0;

    // Called when the attributes (state and/or atr) of a smart card reader
    // changes.
    virtual void OnReaderChanged(
        const blink::mojom::SmartCardReaderInfo& reader_info) = 0;

    // Called when a error preventing the monitoring of reader changes occurs.
    // Can be retried with a new `Start` call.
    virtual void OnError(device::mojom::SmartCardError error) = 0;
  };

  class ObserverList {
   public:
    ObserverList();
    ObserverList(const ObserverList&) = delete;
    ObserverList& operator=(const ObserverList&) = delete;
    ~ObserverList();

    bool empty() const { return observers_.empty(); }

    void AddObserverIfMissing(Observer* observer);
    void RemoveObserver(Observer* observer);

    void NotifyReaderAdded(
        const blink::mojom::SmartCardReaderInfo& reader_info);
    void NotifyReaderChanged(
        const blink::mojom::SmartCardReaderInfo& reader_info);
    void NotifyReaderRemoved(
        const blink::mojom::SmartCardReaderInfo& reader_info);
    void NotifyError(device::mojom::SmartCardError error);

   private:
    base::ObserverList<Observer> observers_;
  };

  static SmartCardReaderTracker& GetForBrowserContext(BrowserContext&,
                                                      SmartCardDelegate&);

  using StartCallback =
      base::OnceCallback<void(blink::mojom::SmartCardGetReadersResultPtr)>;

  SmartCardReaderTracker() = default;
  ~SmartCardReaderTracker() override = default;

  // Returns the list of currently available smart card readers and (re)starts
  // tracking them for changes or removals. If supported, also starts tracking
  // the addition of new readers.
  //
  // It will stop tracking once there are no more observers or upon the first
  // error encountered.
  virtual void Start(Observer* observer, StartCallback) = 0;

  // Removes an observer and stops tracking smart card reader
  // changes/additions/removals if there are no other observers left
  virtual void Stop(Observer* observer) = 0;

  // Returns the key used in SupportsUserData::GetUserData to fetch
  // the SmartCardReaderTracker associated with a BrowserContext.
  static const void* user_data_key_for_testing();
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_H_
