// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioMirroringManager is a singleton object that maintains a set of active
// audio mirroring destinations and auto-connects/disconnects audio streams
// to/from those destinations.
//
// How it works:
//
//   1. AudioRendererHost gets a CreateStream message from the render process
//      and, among other things, creates an AudioOutputController to control the
//      audio data flow between the render and browser processes.  More
//      importantly, it registers the AudioOutputController with
//      AudioMirroringManager (as a Diverter).
//   2. A user request to mirror all the audio for a WebContents is made.  A
//      MirroringDestination is created, and StartMirroring() is called to begin
//      the mirroring session.  The MirroringDestination is queried to determine
//      which of all the known Diverters will re-route their audio to it.
//   3a. During a mirroring session, AudioMirroringManager may encounter new
//       Diverters, and will query all the MirroringDestinations to determine
//       which is a match, if any.
//   3b. During a mirroring session, a call to StartMirroring() can be made to
//       request a "refresh" query on a MirroringDestination, and this will
//       result in AudioMirroringManager starting/stopping only those Diverters
//       that are not correctly routed to the destination.
//   3c. When a mirroring session is stopped, the remaining destinations will be
//       queried to determine whether diverting should continue to a different
//       destination.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_AUDIO_MIRRORING_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_AUDIO_MIRRORING_MANAGER_H_

#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "media/audio/audio_source_diverter.h"

namespace media {
class AudioOutputStream;
}

namespace content {

class CONTENT_EXPORT AudioMirroringManager {
 public:
  // Interface for diverting audio data to an alternative AudioOutputStream.
  typedef media::AudioSourceDiverter Diverter;

  // Interface to be implemented by audio mirroring destinations.  See comments
  // for StartMirroring() and StopMirroring() below.
  class MirroringDestination {
   public:
    // Asynchronously query whether this MirroringDestination wants to consume
    // audio sourced from each of the |candidates|.  |results_callback| is run
    // to indicate which of them (or none) should have audio routed to this
    // MirroringDestination.  The second parameter of |results_callback|
    // indicates whether the MirroringDestination wants either: 1) exclusive
    // access to a diverted audio flow versus 2) a duplicate copy of the audio
    // flow. |results_callback| must be run on the same thread as the one that
    // called QueryForMatches().
    typedef base::OnceCallback<void(const std::set<GlobalFrameRoutingId>&,
                                    bool)>
        MatchesCallback;
    virtual void QueryForMatches(
        const std::set<GlobalFrameRoutingId>& candidates,
        MatchesCallback results_callback) = 0;

    // Create a consumer of audio data in the format specified by |params|, and
    // connect it as an input to mirroring.  This is used to provide
    // MirroringDestination with exclusive access to pull the audio flow from
    // the source. When Close() is called on the returned AudioOutputStream, the
    // input is disconnected and the object becomes invalid.
    virtual media::AudioOutputStream* AddInput(
        const media::AudioParameters& params) = 0;

    // Create a consumer of audio data in the format specified by |params|, and
    // connect it as an input to mirroring.  This is used to provide
    // MirroringDestination with duplicate audio data, which is pushed from the
    // main audio flow. When Close() is called on the returned AudioPushSink,
    // the input is disconnected and the object becomes invalid.
    virtual media::AudioPushSink* AddPushInput(
        const media::AudioParameters& params) = 0;

   protected:
    virtual ~MirroringDestination() {}
  };

  // Note: Use GetInstance() for non-test code.
  AudioMirroringManager();
  virtual ~AudioMirroringManager();

  // Returns the global instance.
  static AudioMirroringManager* GetInstance();

  // Add/Remove a diverter for an audio stream with a known RenderFrame source
  // (represented by |render_process_id| + |render_frame_id|).  Multiple
  // diverters may be added for the same source frame, but never the same
  // diverter.  |diverter| must live until after RemoveDiverter() is called.
  virtual void AddDiverter(int render_process_id, int render_frame_id,
                           Diverter* diverter);
  virtual void RemoveDiverter(Diverter* diverter);

  // (Re-)Start/Stop mirroring to the given |destination|.  |destination| must
  // live until after StopMirroring() is called.  A client may request a
  // re-start by calling StartMirroring() again; and this will cause
  // AudioMirroringManager to query |destination| and only re-route those
  // diverters that are missing/new to the returned set of matches.
  virtual void StartMirroring(MirroringDestination* destination);
  virtual void StopMirroring(MirroringDestination* destination);

  // TODO(crbug/824019): The following are temporary, as a middle-ground step
  // necessary to resolve a chicken-and-egg problem as we migrate audio
  // mirroring into the new AudioService. See media::AudioManager::AddDiverter()
  // comments for further details.
  using AddDiverterCallback =
      base::RepeatingCallback<void(const base::UnguessableToken&,
                                   media::AudioSourceDiverter*)>;
  AddDiverterCallback GetAddDiverterCallback();
  using RemoveDiverterCallback =
      base::RepeatingCallback<void(media::AudioSourceDiverter*)>;
  RemoveDiverterCallback GetRemoveDiverterCallback();
  static base::UnguessableToken ToGroupId(int render_process_id,
                                          int render_frame_id);

 private:
  friend class AudioMirroringManagerTest;

  struct StreamRoutingState {
    // The source render frame associated with the audio stream.
    GlobalFrameRoutingId source_render_frame;

    // The diverter for re-routing the audio stream.
    Diverter* diverter;

    // If not NULL, the audio stream is currently being diverted to this
    // destination.
    MirroringDestination* destination;

    // The destinations to which audio stream is duplicated. AudioPushSink is
    // owned by the Diverter, but AudioMirroringManager must guarantee
    // StopDuplicating() is called to release them.
    std::map<MirroringDestination*, media::AudioPushSink*> duplications;

    StreamRoutingState(const GlobalFrameRoutingId& source_frame,
                       Diverter* stream_diverter);
    StreamRoutingState(const StreamRoutingState& other);
    ~StreamRoutingState();
  };

  typedef std::vector<StreamRoutingState> StreamRoutes;
  typedef std::vector<MirroringDestination*> Destinations;

  // Helper to find a destination other than |old_destination| for the given
  // |candidates| to be diverted to.
  void InitiateQueriesToFindNewDestination(
      MirroringDestination* old_destination,
      const std::set<GlobalFrameRoutingId>& candidates);

  // MirroringDestination query callback.  |matches| contains all RenderFrame
  // sources that will be diverted or duplicated to |destination|.
  // If |add_only| is false, then any audio flows currently routed to
  // |destination| but not found in |matches| will be stopped.
  // If |is_duplicate| is true, the audio data flow will be duplicated to the
  // destination instead of diverted.
  void UpdateRoutesToDestination(MirroringDestination* destination,
                                 bool add_only,
                                 const std::set<GlobalFrameRoutingId>& matches,
                                 bool is_duplicate);

  // |matches| contains all RenderFrame sources that will be diverted to
  // |destination|.  If |add_only| is false, then any Diverters currently routed
  // to |destination| but not found in |matches| will be stopped.
  void UpdateRoutesToDivertDestination(
      MirroringDestination* destination,
      bool add_only,
      const std::set<GlobalFrameRoutingId>& matches);

  // |matches| contains all RenderFrame sources that will be duplicated to
  // |destination|.  If |add_only| is false, then any Diverters currently
  // duplicating to |destination| but not found in |matches| will be stopped.
  void UpdateRoutesToDuplicateDestination(
      MirroringDestination* destination,
      bool add_only,
      const std::set<GlobalFrameRoutingId>& matches);

  // Starts diverting audio to the |new_destination|, if not NULL.  Otherwise,
  // stops diverting audio.
  static void RouteDivertedFlow(StreamRoutingState* route,
                                MirroringDestination* new_destination);

  // Routing table.  Contains one entry for each Diverter.
  StreamRoutes routes_;

  // All active mirroring sessions.
  Destinations sessions_;

  // Allows calls to Add/RemoveDiverter() and Start/StopMirroring() to be made
  // from different threads.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(AudioMirroringManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_AUDIO_MIRRORING_MANAGER_H_
