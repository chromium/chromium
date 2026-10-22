// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ACTOR_INTERNALS_ACTOR_INTERNALS_HANDLER_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ACTOR_INTERNALS_ACTOR_INTERNALS_HANDLER_H_

#import <memory>

#import "base/files/file_path.h"
#import "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/core/internals/browser/actor_internals_handler.h"
#import "components/actor/public/mojom/actor_internals.mojom-forward.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "mojo/public/cpp/bindings/pending_remote.h"

@class UIActivityViewController;

namespace web {
class WebState;
}

namespace actor {
class AggregatedJournalFileSerializer;
}

// UI Handler for chrome://actor-internals/ on iOS
class ActorInternalsHandler
    : public actor_internals::ActorInternalsHandler::Delegate,
      public actor::AggregatedJournal::Observer {
 public:
  ActorInternalsHandler(
      web::WebState* web_state,
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
  // Callback invoked when the trace file initialization is complete.
  void TraceFileInitDone(bool success);
  // Opens the share menu to let the user interact with the trace.
  void ShareTraceFile(base::FilePath trace_file);
  // Closes the trace file on the serializer's writer sequence and synchronously
  // destroys `file_journal_serializer_` (its `FileWriter` is destroyed behind
  // the close). `on_file_closed` then runs on the current sequence. No-op when
  // logging is not active.
  void CloseTraceFile(base::OnceClosure on_file_closed);

  // The WebState associated with the page.
  base::WeakPtr<web::WebState> web_state_;
  // The cross-platform handler that displays the journal contents on the page.
  std::unique_ptr<actor_internals::ActorInternalsHandler> handler_;
  // The journal that stores the logs to be shown in chrome://actor-internals.
  // raw_ptr since it's owned by the ActorService, a profile keyed service,
  // which outlives this class.
  raw_ptr<actor::AggregatedJournal> journal_ = nullptr;
  // The current trace file that is being written to.
  base::FilePath current_trace_file_;
  // The serializer that is used to write the entries to the trace file.
  std::unique_ptr<actor::AggregatedJournalFileSerializer>
      file_journal_serializer_;
  // The view controller used to share the trace file.
  UIActivityViewController* __strong activity_view_controller_ = nil;
  base::ScopedObservation<actor::AggregatedJournal,
                          actor::AggregatedJournal::Observer>
      journal_observation_{this};

  base::WeakPtrFactory<ActorInternalsHandler> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ACTOR_INTERNALS_ACTOR_INTERNALS_HANDLER_H_
