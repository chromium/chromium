// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_H_
#define CONTENT_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_H_

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/smart_card.mojom.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom-forward.h"

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
    virtual void OnError(blink::mojom::SmartCardResponseCode response_code) = 0;
  };

  static SmartCardReaderTracker& GetForBrowserContext(BrowserContext&,
                                                      SmartCardDelegate&);

  explicit SmartCardReaderTracker(
      mojo::PendingRemote<device::mojom::SmartCardContextFactory>,
      bool context_supports_reader_added);

  ~SmartCardReaderTracker() override;

  using StartCallback =
      base::OnceCallback<void(blink::mojom::SmartCardGetReadersResultPtr)>;

  // Returns the list of currently available smart card readers and (re)starts
  // tracking them for changes or removals. If supported, also starts tracking
  // the addition of new readers.
  //
  // It will stop tracking once there are no more observers or upon the first
  // error encountered.
  void Start(Observer* observer, StartCallback);

  // Removes an observer and stops tracking smart card reader
  // changes/additions/removals if there are no other observers left
  void Stop(Observer* observer);

 private:
  class State;
  class WaitContext;
  class WaitInitialReaderStatus;
  class Tracking;
  class Uninitialized;
  class WaitReadersList;
  class KeepContext;
  class Reader;

  void AddObserver(Observer* observer);

  void ChangeState(std::unique_ptr<State> next_state);

  void NotifyReaderAdded(const blink::mojom::SmartCardReaderInfo& reader_info);
  void NotifyReaderChanged(
      const blink::mojom::SmartCardReaderInfo& reader_info);
  void NotifyReaderRemoved(
      const blink::mojom::SmartCardReaderInfo& reader_info);
  void NotifyError(blink::mojom::SmartCardResponseCode response_code);

  bool CanTrack() const;

  void AddReader(const device::mojom::SmartCardReaderStateOut& state_out);
  void AddOrUpdateReader(
      const device::mojom::SmartCardReaderStateOut& state_out);
  void RemoveReader(const device::mojom::SmartCardReaderStateOut& state_out);

  void GetReadersFromCache(StartCallback callback);
  void UpdateCache(const std::vector<device::mojom::SmartCardReaderStateOutPtr>&
                       reader_states);

  // Current state.
  std::unique_ptr<State> state_;

  base::ObserverList<Observer> observer_list_;
  mojo::Remote<device::mojom::SmartCardContextFactory> context_factory_;
  std::map<std::string, std::unique_ptr<Reader>> readers_;
  const bool context_supports_reader_added_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_H_
