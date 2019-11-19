// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/audio_mirroring_manager.h"

#include <stdint.h>
#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"

namespace content {

// static
AudioMirroringManager* AudioMirroringManager::GetInstance() {
  static AudioMirroringManager* manager = new AudioMirroringManager();
  return manager;
}

AudioMirroringManager::AudioMirroringManager() {}

AudioMirroringManager::~AudioMirroringManager() {}

void AudioMirroringManager::AddDiverter(
    int render_process_id, int render_frame_id, Diverter* diverter) {
  DCHECK(diverter);

  base::AutoLock scoped_lock(lock_);

  // DCHECK(diverter not already in routes_)
#ifndef NDEBUG
  for (StreamRoutes::const_iterator it = routes_.begin();
       it != routes_.end(); ++it) {
    DCHECK_NE(diverter, it->diverter);
  }
#endif
  routes_.push_back(StreamRoutingState(
      GlobalFrameRoutingId(render_process_id, render_frame_id), diverter));

  // Query existing destinations to see whether to immediately start diverting
  // the stream.
  std::set<GlobalFrameRoutingId> candidates;
  candidates.insert(routes_.back().source_render_frame);
  InitiateQueriesToFindNewDestination(nullptr, candidates);
}

void AudioMirroringManager::RemoveDiverter(Diverter* diverter) {
  base::AutoLock scoped_lock(lock_);

  // Find and remove the entry from the routing table.  If the stream is being
  // diverted, it is stopped.
  for (auto it = routes_.begin(); it != routes_.end(); ++it) {
    if (it->diverter == diverter) {
      // Stop the diverted flow.
      RouteDivertedFlow(&(*it), nullptr);

      // Stop duplication flows.
      for (auto& dup : it->duplications) {
        diverter->StopDuplicating(dup.second);
      }
      routes_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

void AudioMirroringManager::StartMirroring(MirroringDestination* destination) {
  DCHECK(destination);

  base::AutoLock scoped_lock(lock_);

  // Insert an entry into the set of active mirroring sessions, if this is a
  // previously-unknown destination.
  if (!base::Contains(sessions_, destination))
    sessions_.push_back(destination);

  std::set<GlobalFrameRoutingId> candidates;

  // Query the MirroringDestination to see which of the audio streams should be
  // diverted.
  for (StreamRoutes::const_iterator it = routes_.begin(); it != routes_.end();
       ++it) {
      candidates.insert(it->source_render_frame);
  }
  if (!candidates.empty()) {
    destination->QueryForMatches(
        candidates,
        base::BindOnce(&AudioMirroringManager::UpdateRoutesToDestination,
                       base::Unretained(this), destination, false));
  }
}

void AudioMirroringManager::StopMirroring(MirroringDestination* destination) {
  base::AutoLock scoped_lock(lock_);

  // Stop diverting each audio stream in the mirroring session being stopped.
  // Each stopped stream becomes a candidate to be diverted to another
  // destination.
  std::set<GlobalFrameRoutingId> redivert_candidates;
  for (auto it = routes_.begin(); it != routes_.end(); ++it) {
    if (it->destination == destination) {
      RouteDivertedFlow(&(*it), nullptr);
      redivert_candidates.insert(it->source_render_frame);
    }
    auto dup_it = it->duplications.find(destination);
    if (dup_it != it->duplications.end()) {
      it->diverter->StopDuplicating(dup_it->second);
      it->duplications.erase(dup_it);
    }
  }
  if (!redivert_candidates.empty())
    InitiateQueriesToFindNewDestination(destination, redivert_candidates);

  // Remove the entry from the set of active mirroring sessions.
  const Destinations::iterator dest_it =
      std::find(sessions_.begin(), sessions_.end(), destination);
  if (dest_it == sessions_.end()) {
    NOTREACHED();
    return;
  }
  sessions_.erase(dest_it);
}

AudioMirroringManager::AddDiverterCallback
AudioMirroringManager::GetAddDiverterCallback() {
  return base::BindRepeating(
      [](AudioMirroringManager* self,
         const base::UnguessableToken& guessable_token, Diverter* diverter) {
        const int render_process_id =
            static_cast<int>(guessable_token.GetHighForSerialization());
        const int render_frame_id =
            static_cast<int>(guessable_token.GetLowForSerialization());
        self->AddDiverter(render_process_id, render_frame_id, diverter);
      },
      base::Unretained(this));
}

AudioMirroringManager::RemoveDiverterCallback
AudioMirroringManager::GetRemoveDiverterCallback() {
  return base::BindRepeating(&AudioMirroringManager::RemoveDiverter,
                             base::Unretained(this));
}

// static
base::UnguessableToken AudioMirroringManager::ToGroupId(int render_process_id,
                                                        int render_frame_id) {
  return base::UnguessableToken::Deserialize(
      static_cast<uint64_t>(render_process_id),
      static_cast<uint64_t>(render_frame_id));
}

void AudioMirroringManager::InitiateQueriesToFindNewDestination(
    MirroringDestination* old_destination,
    const std::set<GlobalFrameRoutingId>& candidates) {
  lock_.AssertAcquired();

  for (Destinations::const_iterator it = sessions_.begin();
       it != sessions_.end(); ++it) {
    if (*it == old_destination)
      continue;

    (*it)->QueryForMatches(
        candidates,
        base::BindOnce(&AudioMirroringManager::UpdateRoutesToDestination,
                       base::Unretained(this), *it, true));
  }
}

void AudioMirroringManager::UpdateRoutesToDestination(
    MirroringDestination* destination,
    bool add_only,
    const std::set<GlobalFrameRoutingId>& matches,
    bool is_duplicate) {
  base::AutoLock scoped_lock(lock_);

  if (is_duplicate)
    UpdateRoutesToDuplicateDestination(destination, add_only, matches);
  else
    UpdateRoutesToDivertDestination(destination, add_only, matches);
}

void AudioMirroringManager::UpdateRoutesToDivertDestination(
    MirroringDestination* destination,
    bool add_only,
    const std::set<GlobalFrameRoutingId>& matches) {
  lock_.AssertAcquired();

  if (!base::Contains(sessions_, destination))
    return;  // Query result callback invoked after StopMirroring().

  DVLOG(1) << (add_only ? "Add " : "Replace with ") << matches.size()
           << " routes to MirroringDestination@" << destination;

  // Start/stop diverting based on |matches|.  Any stopped stream becomes a
  // candidate to be diverted to another destination.
  std::set<GlobalFrameRoutingId> redivert_candidates;
  for (auto it = routes_.begin(); it != routes_.end(); ++it) {
    if (matches.find(it->source_render_frame) != matches.end()) {
      // Only change the route if the stream is not already being diverted.
      if (!it->destination)
        RouteDivertedFlow(&(*it), destination);
    } else if (!add_only) {
      // Only stop diverting if the stream is currently routed to |destination|.
      if (it->destination == destination) {
        RouteDivertedFlow(&(*it), nullptr);
        redivert_candidates.insert(it->source_render_frame);
      }
    }
  }
  if (!redivert_candidates.empty())
    InitiateQueriesToFindNewDestination(destination, redivert_candidates);
}

void AudioMirroringManager::UpdateRoutesToDuplicateDestination(
    MirroringDestination* destination,
    bool add_only,
    const std::set<GlobalFrameRoutingId>& matches) {
  lock_.AssertAcquired();

  if (!base::Contains(sessions_, destination))
    return;  // Query result callback invoked after StopMirroring().

  for (auto it = routes_.begin(); it != routes_.end(); ++it) {
    if (matches.find(it->source_render_frame) != matches.end()) {
      // The same destination cannot have both a diverted audio flow and a
      // duplicated flow from the same source.
      DCHECK_NE(it->destination, destination);

      media::AudioPushSink*& pusher = it->duplications[destination];
      if (!pusher) {
        pusher = destination->AddPushInput(it->diverter->GetAudioParameters());
        DCHECK(pusher);
        it->diverter->StartDuplicating(pusher);
      }
    } else if (!add_only) {
      auto dup_it = it->duplications.find(destination);
      if (dup_it != it->duplications.end()) {
        it->diverter->StopDuplicating(dup_it->second);
        it->duplications.erase(dup_it);
      }
    }
  }
}

// static
void AudioMirroringManager::RouteDivertedFlow(
    StreamRoutingState* route,
    MirroringDestination* new_destination) {
  // The same destination cannot have both a diverted audio flow and a
  // duplicated flow from the same source.
  DCHECK(route->duplications.find(new_destination) ==
         route->duplications.end());

  if (route->destination == new_destination)
    return;  // No change.

  if (route->destination) {
    DVLOG(1) << "Stop diverting render_process_id:render_frame_id="
             << route->source_render_frame.child_id << ':'
             << route->source_render_frame.frame_routing_id
             << " --> MirroringDestination@" << route->destination;
    route->diverter->StopDiverting();
    route->destination = nullptr;
  }

  if (new_destination) {
    DVLOG(1) << "Start diverting of render_process_id:render_frame_id="
             << route->source_render_frame.child_id << ':'
             << route->source_render_frame.frame_routing_id
             << " --> MirroringDestination@" << new_destination;
    route->diverter->StartDiverting(
        new_destination->AddInput(route->diverter->GetAudioParameters()));
    route->destination = new_destination;
  }
}

AudioMirroringManager::StreamRoutingState::StreamRoutingState(
    const GlobalFrameRoutingId& source_frame,
    Diverter* stream_diverter)
    : source_render_frame(source_frame),
      diverter(stream_diverter),
      destination(nullptr) {}

AudioMirroringManager::StreamRoutingState::StreamRoutingState(
    const StreamRoutingState& other) = default;

AudioMirroringManager::StreamRoutingState::~StreamRoutingState() {}

}  // namespace content
