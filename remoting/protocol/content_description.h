// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONTENT_DESCRIPTION_H_
#define REMOTING_PROTOCOL_CONTENT_DESCRIPTION_H_

#include <memory>
#include <string>

#include "remoting/protocol/session_config.h"

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace remoting::protocol {

// ContentDescription used for chromoting sessions. It contains the information
// from the content description stanza in the session initialization handshake.
//
// This class also provides a type abstraction so that the Chromotocol Session
// interface does not need to depend on libjingle.
class ContentDescription {
 public:
  static const char kChromotingContentName[];

  ContentDescription(
      std::unique_ptr<CandidateSessionConfig> config,
      std::unique_ptr<jingle_xmpp::XmlElement> authenticator_message);
  ~ContentDescription();

  const CandidateSessionConfig* config() const {
    return candidate_config_.get();
  }

  const jingle_xmpp::XmlElement* authenticator_message() const {
    return authenticator_message_.get();
  }

  jingle_xmpp::XmlElement* ToXml() const;

  static std::unique_ptr<ContentDescription> ParseXml(
      const jingle_xmpp::XmlElement* element,
      bool webrtc_transport);

 private:
  std::unique_ptr<const CandidateSessionConfig> candidate_config_;
  std::unique_ptr<const jingle_xmpp::XmlElement> authenticator_message_;

  static bool ParseChannelConfigs(const jingle_xmpp::XmlElement* const element,
                                  const char tag_name[],
                                  bool codec_required,
                                  bool optional,
                                  std::list<ChannelConfig>* const configs);
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_CONTENT_DESCRIPTION_H_
