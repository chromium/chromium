// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_SIGNALING_ADDRESS_H_
#define REMOTING_SIGNALING_SIGNALING_ADDRESS_H_

#include <string>

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace remoting {

// Represents an address of a Chromoting endpoint and its routing channel.
class SignalingAddress {
 public:
  enum class Channel { XMPP, FTL };
  enum Direction { TO, FROM };
  // Creates an empty SignalingAddress.
  SignalingAddress();

  // Creates a SignalingAddress with |id|, which can either be a valid FTL ID,
  // or XMPP JID.
  explicit SignalingAddress(const std::string& id);

  // Creates a SignalingAddress that represents an FTL endpoint. Note that the
  // FTL SignalingAddress is irrelevant to the FTL server or client. It is just
  // to make existing Jingle session logic work with the new messaging service.
  static SignalingAddress CreateFtlSignalingAddress(
      const std::string& username,
      const std::string& registration_id);

  static SignalingAddress Parse(const jingle_xmpp::XmlElement* iq,
                                Direction direction,
                                std::string* error);

  void SetInMessage(jingle_xmpp::XmlElement* message,
                    Direction direction) const;

  // Writes FTL info to |username| and |registration_id|. Returns true if the
  // SIgnalingAddress is a valid FTL address and info is successfully written.
  // If this returns false then none of the parameters will be touched.
  bool GetFtlInfo(std::string* username, std::string* registration_id) const;

  Channel channel() const { return channel_; }
  const std::string& id() const { return id_; }

  bool empty() const { return id_.empty(); }

  bool operator==(const SignalingAddress& other) const;
  bool operator!=(const SignalingAddress& other) const;

 private:
  // Represents the |to| or |from| field in an IQ stanza.
  std::string id_;

  Channel channel_ = Channel::XMPP;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_SIGNALING_ADDRESS_H_
