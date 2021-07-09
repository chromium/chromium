// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/signaling_address.h"

#include <string.h>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "remoting/base/name_value_map.h"
#include "remoting/signaling/signaling_id_util.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

namespace {

// FTL ID format:
// user@domain.com/chromoting_ftl_(registration ID)
// The FTL ID is only used local to the program.
constexpr char kFtlResourcePrefix[] = "chromoting_ftl_";

jingle_xmpp::QName GetIdQName(SignalingAddress::Direction direction) {
  const char* attribute_name = attribute_name =
      (direction == SignalingAddress::FROM) ? "from" : "to";
  return jingle_xmpp::QName("", attribute_name);
}

SignalingAddress::Channel GetChannelType(std::string address) {
  std::string email;
  std::string resource;
  bool has_resource = SplitSignalingIdResource(address, &email, &resource);
  if (has_resource) {
    if (resource.find(kFtlResourcePrefix) == 0) {
      return SignalingAddress::Channel::FTL;
    }
  }
  return SignalingAddress::Channel::XMPP;
}

}  // namespace

SignalingAddress::SignalingAddress() = default;

SignalingAddress::SignalingAddress(const std::string& address) {
  DCHECK(!address.empty());
  channel_ = GetChannelType(address);
  switch (channel_) {
    case Channel::XMPP:
    case Channel::FTL:
      id_ = NormalizeSignalingId(address);
      DCHECK(!id_.empty()) << "Missing signaling ID.";
      break;
    default:
      NOTREACHED();
  }
}

bool SignalingAddress::operator==(const SignalingAddress& other) const {
  return (other.id_ == id_) && (other.channel_ == channel_);
}

bool SignalingAddress::operator!=(const SignalingAddress& other) const {
  return !(*this == other);
}

// static
SignalingAddress SignalingAddress::CreateFtlSignalingAddress(
    const std::string& username,
    const std::string& registration_id) {
  return SignalingAddress(base::StringPrintf("%s/%s%s", username.c_str(),
                                             kFtlResourcePrefix,
                                             registration_id.c_str()));
}

// static
SignalingAddress SignalingAddress::Parse(const jingle_xmpp::XmlElement* iq,
                                         SignalingAddress::Direction direction,
                                         std::string* error) {
  std::string id = iq->Attr(GetIdQName(direction));
  if (id.empty()) {
    return SignalingAddress();
  }
  return SignalingAddress(id);
}

void SignalingAddress::SetInMessage(jingle_xmpp::XmlElement* iq,
                                    Direction direction) const {
  DCHECK(!empty()) << "Signaling Address is empty";
  iq->SetAttr(GetIdQName(direction), id_);
}

bool SignalingAddress::GetFtlInfo(std::string* username,
                                  std::string* registration_id) const {
  if (channel_ != Channel::FTL) {
    return false;
  }
  std::string resource;
  bool has_resource = SplitSignalingIdResource(id_, username, &resource);
  DCHECK(has_resource);
  size_t ftl_resource_prefix_length = strlen(kFtlResourcePrefix);
  DCHECK_LT(ftl_resource_prefix_length, resource.length());
  *registration_id = resource.substr(ftl_resource_prefix_length);
  return true;
}

}  // namespace remoting
