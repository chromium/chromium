// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUE_LOG_EVENT_BUILDER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUE_LOG_EVENT_BUILDER_H_

#import <string>
#import <vector>

#import "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#import "components/private_insights/events/contextual_cue_log_event.pb.h"
#import "url/gurl.h"

namespace web {
class WebState;
}  // namespace web

namespace contextual_cueing {

// Everything needed to build the Private Insights events of one contextual
// cue presentation, captured when the cue is shown. A CLICKED event is built
// from the same snapshot as its SHOWN event, so both carry the same `cue_id`
// and page context even if a navigation has invalidated the cue in between.
struct ContextualCueSnapshot {
  ContextualCueSnapshot();
  ContextualCueSnapshot(const ContextualCueSnapshot&);
  ContextualCueSnapshot(ContextualCueSnapshot&&);
  ContextualCueSnapshot& operator=(const ContextualCueSnapshot&);
  ContextualCueSnapshot& operator=(ContextualCueSnapshot&&);
  ~ContextualCueSnapshot();

  // Random UUID shared by all events of this presentation.
  std::string cue_id;
  // The cue as returned by the server.
  optimization_guide::proto::ContextualCue cue;
  // Last committed URL and title of the page the cue was shown on.
  GURL active_page_url;
  std::string active_page_title;
  // Background tabs that were sent with the `ContextualCueingRequest`.
  std::vector<optimization_guide::proto::Tab> background_tabs;
};

// Returns a snapshot of `cue` presented on the last committed page of
// `web_state`, with a freshly generated `cue_id`.
ContextualCueSnapshot CreateContextualCueSnapshot(
    web::WebState* web_state,
    const optimization_guide::proto::ContextualCue& cue,
    std::vector<optimization_guide::proto::Tab> background_tabs);

// Builds the Private Insights event of `event_type` for `snapshot`, stamped
// with the current time and the iOS `CoarseSystemProfile` (version, milestone,
// channel, platform "iOS", application locale and variations country).
private_insights::events::ContextualCueLogEvent CreateContextualCueLogEvent(
    private_insights::events::ContextualCueLogEvent::EventType event_type,
    const ContextualCueSnapshot& snapshot);

}  // namespace contextual_cueing

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUE_LOG_EVENT_BUILDER_H_
