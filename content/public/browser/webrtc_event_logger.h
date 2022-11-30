// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBRTC_EVENT_LOGGER_H_
#define CONTENT_PUBLIC_BROWSER_WEBRTC_EVENT_LOGGER_H_

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

  // Enable local logging of WebRTC events.
  // Local logging is distinguished from remote logging, in that local logs are
  // kept in response to explicit user input, are saved to a specific location,
  // and are never removed by the application. Logging is done on a best-effort
  // basis. An illegal file path, or one where we don't have the necessary
  // permissions, will result in a failure to log WebRTC events. Also, there is
  // a limit on the number of files and on the length of the log filenames that
  // could also prevent the logging from happening. If the number of currently
  // active peer connections exceeds the maximum number of local log files,
  // there is no guarantee about which PCs will get a local log file associated
  // (specifically, we do *not* guarantee it would be either the oldest or the
  // newest).
  virtual void EnableLocalLogging(const base::FilePath& base_path) = 0;

  // Disable local logging of WebRTC events.
  // Any active local logs are stopped. Peer connections added after this call
  // will not get a local log associated with them (unless local logging is
  // once again enabled).
  virtual void DisableLocalLogging() = 0;

 protected:
  WebRtcEventLogger();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBRTC_EVENT_LOGGER_H_
