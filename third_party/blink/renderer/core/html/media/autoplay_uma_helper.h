// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_AUTOPLAY_UMA_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_AUTOPLAY_UMA_HELPER_H_

#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

// These values are used for histograms. Do not reorder.
enum class AutoplaySource {
  // Autoplay comes from HTMLMediaElement `autoplay` attribute.
  kAttribute = 0,
  // Autoplay comes from `play()` method.
  kMethod = 1,
  // Used for checking dual source.
  kNumberOfSources = 2,
  // Both sources are used.
  kDualSource = 2,
  // This enum value must be last.
  kNumberOfUmaSources = 3,
};

// These values are used for histograms. Do not reorder.
enum class AutoplayUnmuteActionStatus {
  kFailure = 0,
  kSuccess = 1,
  kNumberOfStatus = 2,
};

// These values are used for histograms. Do not reorder.
enum AutoplayBlockedReason {
  kAutoplayBlockedReasonDataSaver_DEPRECATED = 0,
  kAutoplayBlockedReasonSetting = 1,
  kAutoplayBlockedReasonDataSaverAndSetting_DEPRECATED = 2,
  // Keey at the end.
  kAutoplayBlockedReasonMax = 3
};

class Document;
class HTMLMediaElement;
class IntersectionObserver;
class IntersectionObserverEntry;

class CORE_EXPORT AutoplayUmaHelper : public NativeEventListener,
                                      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(AutoplayUmaHelper);

 public:
  explicit AutoplayUmaHelper(HTMLMediaElement*);
  ~AutoplayUmaHelper() override;

  void ContextDestroyed(ExecutionContext*) override;

  void OnAutoplayInitiated(AutoplaySource);

  void RecordAutoplayUnmuteStatus(AutoplayUnmuteActionStatus);

  void VideoWillBeDrawnToCanvas();
  void DidMoveToNewDocument(Document& old_document);

  bool IsVisible() const { return is_visible_; }

  bool HasSource() const { return !sources_.IsEmpty(); }

  void Invoke(ExecutionContext*, Event*) override;

  void Trace(Visitor*) override;

 private:
  friend class MockAutoplayUmaHelper;

  // Called when source is initialized and loading starts.
  void OnLoadStarted();

  void HandlePlayingEvent();
  void HandlePauseEvent();
  virtual void HandleContextDestroyed();  // Make virtual for testing.

  void MaybeUnregisterContextDestroyedObserver();
  void MaybeUnregisterMediaElementPauseListener();

  void MaybeStartRecordingMutedVideoPlayMethodBecomeVisible();
  void MaybeStopRecordingMutedVideoPlayMethodBecomeVisible(bool is_visible);

  void MaybeStartRecordingMutedVideoOffscreenDuration();
  void MaybeStopRecordingMutedVideoOffscreenDuration();

  void OnIntersectionChangedForMutedVideoOffscreenDuration(
      const HeapVector<Member<IntersectionObserverEntry>>& entries);
  void OnIntersectionChangedForMutedVideoPlayMethodBecomeVisible(
      const HeapVector<Member<IntersectionObserverEntry>>& entries);

  bool ShouldListenToContextDestroyed() const;

  // The autoplay sources.
  HashSet<AutoplaySource> sources_;

  // The media element this UMA helper is attached to. |element| owns |this|.
  Member<HTMLMediaElement> element_;

  // The observer is used to observe whether a muted video autoplaying by play()
  // method become visible at some point.
  // The UMA is pending for recording as long as this observer is non-null.
  Member<IntersectionObserver> muted_video_play_method_intersection_observer_;

  // -----------------------------------------------------------------------
  // Variables used for recording the duration of autoplay muted video playing
  // offscreen.  The variables are valid when
  // |autoplayOffscrenVisibilityObserver| is non-null.
  // The recording stops whenever the playback pauses or the page is unloaded.

  // The starting time of autoplaying muted video.
  base::TimeTicks muted_video_autoplay_offscreen_start_time_;

  // The duration an autoplaying muted video has been in offscreen.
  base::TimeDelta muted_video_autoplay_offscreen_duration_;

  // Whether an autoplaying muted video is visible.
  bool is_visible_;

  // The observer is used to observer an autoplaying muted video changing it's
  // visibility, which is used for offscreen duration UMA.  The UMA is pending
  // for recording as long as this observer is non-null.
  Member<IntersectionObserver>
      muted_video_offscreen_duration_intersection_observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_AUTOPLAY_UMA_HELPER_H_
