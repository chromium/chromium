// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_STREAM_MONITOR_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_STREAM_MONITOR_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class WebContents;

// Keeps track of the audible state of audio output streams and uses it to
// maintain a "was recently audible" binary state for the audio indicators in
// the tab UI.  The logic is to: 1) Turn on immediately when sound is audible;
// and 2) Hold on for X amount of time after sound has gone silent, then turn
// off if no longer audible.  Said another way, we don't want tab indicators to
// turn on/off repeatedly and annoy the user.  AudioStreamMonitor sends UI
// update notifications only when needed, but may be queried at any time.
//
// When monitoring is not available, audibility is approximated with having
// active audio streams.
//
// Each WebContentsImpl owns an AudioStreamMonitor.
class CONTENT_EXPORT AudioStreamMonitor : public WebContentsObserver {
 public:
  explicit AudioStreamMonitor(WebContents* contents);

  AudioStreamMonitor(const AudioStreamMonitor&) = delete;
  AudioStreamMonitor& operator=(const AudioStreamMonitor&) = delete;

  ~AudioStreamMonitor() override;

  // Returns true if audio has recently been audible from the tab.  This is
  // usually called whenever the tab data model is refreshed; but there are
  // other use cases as well (e.g., the OOM killer uses this to de-prioritize
  // the killing of tabs making sounds).
  bool WasRecentlyAudible() const;

  // Returns true if the audio is currently audible from the given WebContents.
  // The difference from WasRecentlyAudible() is that this method will return
  // false as soon as the WebContents stop producing sound.
  bool IsCurrentlyAudible() const;

  // Called by the WebContentsImpl if |render_process_id| dies; used to clear
  // any outstanding poll callbacks.
  void RenderProcessGone(int render_process_id);

  // Starts or stops monitoring respectively for the stream owned by the
  // specified renderer.  Safe to call from any thread.
  static void StartMonitoringStream(
      GlobalRenderFrameHostId render_frame_host_id,
      int stream_id);
  static void StopMonitoringStream(GlobalRenderFrameHostId render_frame_host_id,
                                   int stream_id);
  // Updates the audible state for the given stream. Safe to call from any
  // thread.
  static void UpdateStreamAudibleState(
      GlobalRenderFrameHostId render_frame_host_id,
      int stream_id,
      bool is_audible);

  // WebContentsObserver implementation
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  // Overloaded to avoid conflict with RenderProcessGone(int).
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {}

  void set_was_recently_audible_for_testing(bool value) {
    indicator_is_on_ = value;
  }

  void set_is_currently_audible_for_testing(bool value) { is_audible_ = value; }

  // Class to help automatically remove audible client.
  class CONTENT_EXPORT AudibleClientRegistration {
   public:
    AudibleClientRegistration(GlobalRenderFrameHostId render_frame_host_id,
                              AudioStreamMonitor* audio_stream_monitor);
    ~AudibleClientRegistration();

   private:
    GlobalRenderFrameHostId render_frame_host_id_;
    raw_ptr<AudioStreamMonitor> audio_stream_monitor_;
  };

  // Registers an audible client, which will be unregistered when the returned
  // AudibleClientRegistration is released.
  std::unique_ptr<AudibleClientRegistration> RegisterAudibleClient(
      GlobalRenderFrameHostId render_frame_host_id);

 private:
  friend class AudioStreamMonitorTest;
  friend class AudibleClientRegistration;

  enum {
    // Minimum amount of time to hold a tab indicator on after it becomes
    // silent.
    kHoldOnMilliseconds = 2000
  };

  struct CONTENT_EXPORT StreamID {
    GlobalRenderFrameHostId render_frame_host_id;
    int stream_id;
    bool operator<(const StreamID& other) const;
    bool operator==(const StreamID& other) const;
  };

  // Starts monitoring the audible state for the given stream.
  void StartMonitoringStreamOnUIThread(const StreamID& sid);

  // Stops monitoring the audible state for the given stream.
  void StopMonitoringStreamOnUIThread(const StreamID& sid);

  // Updates the audible state for the given stream.
  void UpdateStreamAudibleStateOnUIThread(const StreamID& sid, bool is_audible);

  // Compares last known indicator state with what it should be, and triggers UI
  // updates through |web_contents_| if needed.  When the indicator is turned
  // on, |off_timer_| is started to re-invoke this method in the future.
  void MaybeToggle();
  void UpdateStreams();

  // Adds/Removes Audible clients.
  void AddAudibleClient(GlobalRenderFrameHostId render_frame_host_id);
  void RemoveAudibleClient(GlobalRenderFrameHostId render_frame_host_id);

  // The WebContents instance to receive indicator toggle notifications. This
  // pointer should be valid for the lifetime of AudioStreamMonitor.
  const raw_ptr<WebContents> web_contents_;

  // Confirms single-threaded access in debug builds.
  base::ThreadChecker thread_checker_;

  // The audible state for each stream.  Only playing (i.e., not paused)
  // streams will have an entry in this map.
  base::flat_map<StreamID, bool> streams_;

  // Map of non-stream audible clients, e.g. players not using AudioServices.
  // size_t is the number of audible clients associated with the
  // GlobalRenderFrameHostId. If size_t count reaches 0 there are no
  // remaining audible clients for the associated host id.
  base::flat_map<GlobalRenderFrameHostId, size_t> audible_clients_;

  // Records the last time at which all streams became silent.
  base::TimeTicks last_became_silent_time_;

  // Set to true if the last call to MaybeToggle() determined the indicator
  // should be turned on.
  bool indicator_is_on_ = false;

  // Whether the WebContents is currently audible.
  bool is_audible_ = false;

  // Started only when an indicator is toggled on, to turn it off again in the
  // future.
  base::OneShotTimer off_timer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_STREAM_MONITOR_H_
