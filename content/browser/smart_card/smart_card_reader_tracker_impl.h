// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_IMPL_H_
#define CONTENT_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_IMPL_H_

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "content/browser/smart_card/smart_card_reader_tracker.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/smart_card.mojom.h"

namespace content {
class CONTENT_EXPORT SmartCardReaderTrackerImpl
    : public SmartCardReaderTracker {
 public:
  SmartCardReaderTrackerImpl(
      mojo::PendingRemote<device::mojom::SmartCardContextFactory>,
      bool context_supports_reader_added);

  ~SmartCardReaderTrackerImpl() override;

  // `SmartCardReaderTracker` overrides:
  void Start(Observer* observer, StartCallback) override;
  void Stop(Observer* observer) override;

 private:
  class State;
  class WaitContext;
  class WaitInitialReaderStatus;
  class Tracking;
  class Uninitialized;
  class WaitReadersList;
  class KeepContext;
  class Reader;

  void ChangeState(std::unique_ptr<State> next_state);
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

  ObserverList observer_list_;
  mojo::Remote<device::mojom::SmartCardContextFactory> context_factory_;
  std::map<std::string, std::unique_ptr<Reader>> readers_;
  const bool context_supports_reader_added_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_IMPL_H_
