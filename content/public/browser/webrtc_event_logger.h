// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBRTC_EVENT_LOGGER_H_
#define CONTENT_PUBLIC_BROWSER_WEBRTC_EVENT_LOGGER_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

// Interface for a logger of WebRTC events, which the embedding application may
// subclass and instantiate. Only one instance may ever be created, and it must
// live until the embedding application terminates.
class CONTENT_EXPORT WebRtcEventLogger {
 public:
  // Get the only instance of WebRtcEventLogger, if one was instantiated, or
  // nullptr otherwise.
  static WebRtcEventLogger* Get();

  // The embedding application may leak or destroy on shutdown. Either way,
  // it may only be done on shutdown. It's up to the embedding application to
  // only destroy at a time during shutdown when it is guaranteed that tasks
  // posted earlier with a reference to the WebRtcEventLogger object, will
  // not execute.
  virtual ~WebRtcEventLogger();

  // Call this to let the logger know when a PeerConnection was created.
  // If a reply callback is given, it will be posted back to BrowserThread::UI,
  // with true if and only if the operation was successful (failure is only
  // possible if a peer connection with this exact key was previously added,
  // but not removed).
  virtual void PeerConnectionAdded(const GlobalFrameRoutingId& frame_id,
                                   int lid,
                                   base::OnceCallback<void(bool)> reply) = 0;

  // Call this to let the logger know when a PeerConnection was closed.
  // If a reply callback is given, it will be posted back to BrowserThread::UI,
  // with true if and only if the operation was successful (failure is only
  // possible if a peer connection with this key was not previously added,
  // or if it has since already been removed).
  virtual void PeerConnectionRemoved(const GlobalFrameRoutingId& frame_id,
                                     int lid,
                                     base::OnceCallback<void(bool)> reply) = 0;

  // Call this to let the logger know when a PeerConnection was stopped.
  // Closing of a peer connection is an irreversible action. Its distinction
  // from the removal event is that it may happen before the peer connection has
  // been garbage collected.
  virtual void PeerConnectionStopped(const GlobalFrameRoutingId& frame_id,
                                     int lid,
                                     base::OnceCallback<void(bool)> reply) = 0;

  // Call this to let the logger know of a peer connection's session
  // description ID. By referring to this ID, remote-bound event logging
  // may later be initiated for the peer connection.
  virtual void PeerConnectionSessionIdSet(
      const GlobalFrameRoutingId& frame_id,
      int lid,
      const std::string& session_id,
      base::OnceCallback<void(bool)> reply) = 0;

  // Enable local logging of WebRTC events.
  // Local logging is distinguished from remote logging, in that local logs are
  // kept in response to explicit user input, are saved to a specific location,
  // and are never removed by the application.
  // If a reply callback is given, it will be posted back to BrowserThread::UI,
  // with true if and only if local logging was *not* already on.
  // Note #1: An illegal file path, or one where we don't have the necessary
  // permissions, does not cause a |false| reply, since even when we have the
  // permissions, we're not guaranteed to keep them, and some files might be
  // legal while others aren't due to additional restrictions (number of files,
  // length of filename, etc.).
  // Note #2: If the number of currently active peer connections exceeds the
  // maximum number of local log files, there is no guarantee about which PCs
  // will get a local log file associated (specifically, we do *not* guarantee
  // it would be either the oldest or the newest).
  virtual void EnableLocalLogging(const base::FilePath& base_path,
                                  base::OnceCallback<void(bool)> reply) = 0;

  // Disable local logging of WebRTC events.
  // Any active local logs are stopped. Peer connections added after this call
  // will not get a local log associated with them (unless local logging is
  // once again enabled).
  virtual void DisableLocalLogging(base::OnceCallback<void(bool)> reply) = 0;

  // Called when a new log fragment is sent from the renderer. This will
  // potentially be written to a local WebRTC event log, a remote-bound log
  // intended for upload, or both.
  // If a reply callback is given, it will be posted back to BrowserThread::UI
  // with a pair of bools, the first bool associated with local logging and the
  // second bool associated with remote-bound logging. Each bool assumes the
  // value true if and only if the message was written in its entirety into
  // a local/remote-bound log file.
  virtual void OnWebRtcEventLogWrite(
      const GlobalFrameRoutingId& frame_id,
      int lid,
      const std::string& message,
      base::OnceCallback<void(std::pair<bool, bool>)> reply) = 0;

 protected:
  WebRtcEventLogger();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBRTC_EVENT_LOGGER_H_
