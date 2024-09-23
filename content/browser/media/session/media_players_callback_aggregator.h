// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_PLAYERS_CALLBACK_AGGREGATOR_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_PLAYERS_CALLBACK_AGGREGATOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace content {

// This class is used by the `MediaSessionImpl` to aggregate multiple callbacks
// that are sent to the media player implementation on the renderer, as a result
// of a media session request.
//
// Once all callbacks are evaluated, or during callback evaluation (depending on
// method implementation), the `MediaPlayersCallbackAggregator` will reply to
// the browser with the final answer.
//
// In order to keep the aggregator alive, you must create the aggregator
// using scoped_refptr s, and bind the desired aggregator method using a strong
// reference. Once all callbacks sent to the renderer finish their execution,
// and if not already done, the aggregator will send a reply to the browser with
// the resulting aggregated value.
//
// Sample usage:
//
//  // Create the aggregator, passing the callback that will be executed (if it
//  is not null), once all tasks sent to the renderer finish executing.
//  scoped_refptr<MediaPlayersCallbackAggregator> aggregator =
//    MakeRefCounted<MediaPlayersCallbackAggregator>(std::move(callback));
//
//  // Iterate through the desired players, creating the tasks that will be sent
//  to the renderer.
//  for (const auto player : normal_players_) {
//    base::OnceCallback<void(bool)> task =
//      aggregator->CreateVisibilityCallback();
//    player.first.observer->OnRequestVisibility(player.first.player_id,
//                                              std::move(task));
//  }
class CONTENT_EXPORT MediaPlayersCallbackAggregator
    : public base::RefCountedThreadSafe<MediaPlayersCallbackAggregator> {
 public:
  // Individual callbacks to ask the renderer for player's video visibility.
  using VisibilityCb = base::OnceCallback<void(bool)>;

  // Aggregate callback. Run once if any player returns `true` for
  // `meets_visibility_threshold`, or at destruction time if no player returns
  // `true` for `meets_visibility_threshold`.
  using ReportVisibilityCb = base::OnceCallback<void(bool)>;

  explicit MediaPlayersCallbackAggregator(
      ReportVisibilityCb report_visibility_cb);
  MediaPlayersCallbackAggregator() = delete;
  MediaPlayersCallbackAggregator(const MediaPlayersCallbackAggregator&) =
      delete;
  MediaPlayersCallbackAggregator(MediaPlayersCallbackAggregator&&) = delete;
  MediaPlayersCallbackAggregator& operator=(
      const MediaPlayersCallbackAggregator&) = delete;

  // Creates and returns a `VisibilityCb`. The resulting callback holds a strong
  // reference to the aggregator, therefore it will live until all
  // `VisibilityCb` s are executed.
  VisibilityCb CreateVisibilityCallback();

 private:
  friend class base::RefCountedThreadSafe<MediaPlayersCallbackAggregator>;
  ~MediaPlayersCallbackAggregator();

  void OnGetVisibility(bool meets_visibility_threshold);

  ReportVisibilityCb report_visibility_cb_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_PLAYERS_CALLBACK_AGGREGATOR_H_
