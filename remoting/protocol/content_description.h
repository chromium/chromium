// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONTENT_DESCRIPTION_H_
#define REMOTING_PROTOCOL_CONTENT_DESCRIPTION_H_

#include <memory>
#include <string>

#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/session_config.h"

namespace remoting::protocol {

// ContentDescription used for chromoting sessions. It contains the information
// from the content description stanza in the session initialization handshake.
//
// This class also provides a type abstraction so that the Chromotocol Session
// interface does not need to depend on libjingle.
class ContentDescription {
 public:
  static const char kChromotingContentName[];

  // TODO: joedow - Add a c'tor which accepts a JingleAuthentication&&.
  ContentDescription(std::unique_ptr<CandidateSessionConfig> config,
                     const JingleAuthentication& authentication);
  ~ContentDescription();

  const CandidateSessionConfig* config() const {
    return candidate_config_.get();
  }

  const JingleAuthentication& authentication() const { return authentication_; }

 private:
  std::unique_ptr<const CandidateSessionConfig> candidate_config_;
  JingleAuthentication authentication_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_CONTENT_DESCRIPTION_H_
