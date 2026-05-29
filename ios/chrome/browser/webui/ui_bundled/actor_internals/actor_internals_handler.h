// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ACTOR_INTERNALS_ACTOR_INTERNALS_HANDLER_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ACTOR_INTERNALS_ACTOR_INTERNALS_HANDLER_H_

#import "base/scoped_observation.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/core/internals/browser/actor_internals_handler.h"
#import "components/actor/public/mojom/actor_internals.mojom.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "mojo/public/cpp/bindings/pending_remote.h"

// UI Handler for chrome://actor-internals/ on iOS
class ActorInternalsHandler
    : public actor_internals::ActorInternalsHandler::Delegate,
      public actor::AggregatedJournal::Observer {
 public:
  ActorInternalsHandler(
      actor::AggregatedJournal* journal,
      mojo::PendingRemote<actor_internals::mojom::Page> page,
      mojo::PendingReceiver<actor_internals::mojom::PageHandler> receiver);

  ActorInternalsHandler(const ActorInternalsHandler&) = delete;
  ActorInternalsHandler& operator=(const ActorInternalsHandler&) = delete;

  ~ActorInternalsHandler() override;

  // actor_internals::ActorInternalsHandler::Delegate:
  void StartLogging() override;
  void StopLogging() override;

  // actor::AggregatedJournal::Observer:
  void WillAddJournalEntry(
      const actor::AggregatedJournal::Entry& entry) override;

 private:
  std::unique_ptr<actor_internals::ActorInternalsHandler> handler_;
  base::ScopedObservation<actor::AggregatedJournal,
                          actor::AggregatedJournal::Observer>
      journal_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ACTOR_INTERNALS_ACTOR_INTERNALS_HANDLER_H_
