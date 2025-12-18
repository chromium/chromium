// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/signaling_address.h"

#include <string.h>

#include <ostream>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "remoting/base/name_value_map.h"
#include "remoting/signaling/corp_messaging_constants.h"
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

  // Although a true XMPP JID does not require a 'user' field, every service we
  // interact with does so we can expect that every address passed in will also
  // have one. The current exception is that we have overloaded SignalingAddress
  // to store an authz token for messaging. For now we can assume that no '@'
  // means a CORP address.
  // TODO: joedow - Refactor the SignalingAddress class to store the authz token
  // alongside the user JID and remove this check and assume a missing '@' is
  // 'XMPP' which doesn't have a signaling strategy associated with it.
  auto email_parts = base::SplitStringOnce(email, '@');
  if (!email_parts.has_value()) {
    return SignalingAddress::Channel::CORP;
  }

  auto [_, domain] = *email_parts;
  // Corp signaling addresses will have a format like:
  //   <username>@corp.google.com
  //   <UUID>@<UUID_TYPE>.corp.google.com
  if (base::EndsWith(domain, kCorpSignalingDomain,
                     base::CompareCase::INSENSITIVE_ASCII)) {
    return SignalingAddress::Channel::CORP;
  }

  if (has_resource && resource.starts_with(kFtlResourcePrefix)) {
    return SignalingAddress::Channel::FTL;
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
    case Channel::CORP:
      id_ = address;
      DCHECK(!id_.empty()) << "Missing signaling ID.";
      break;
    default:
      NOTREACHED();
  }
}

// static
SignalingAddress SignalingAddress::CreateFtlSystemAddress(
    const std::string& id) {
  SignalingAddress address;
  address.is_system_ = true;
  address.id_ = id;
  address.channel_ = Channel::FTL;
  return address;
}

bool SignalingAddress::operator==(const SignalingAddress& other) const {
  return (other.id_ == id_) && (other.channel_ == channel_) &&
         (other.is_system_ == is_system_);
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
SignalingAddress SignalingAddress::Parse(
    const jingle_xmpp::XmlElement* iq,
    SignalingAddress::Direction direction) {
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

bool SignalingAddress::GetFtlInfo(std::string* email,
                                  std::string* registration_id) const {
  if (channel_ != Channel::FTL) {
    return false;
  }
  std::string resource;
  bool has_resource = SplitSignalingIdResource(id_, email, &resource);
  DCHECK(has_resource);
  size_t ftl_resource_prefix_length = strlen(kFtlResourcePrefix);
  DCHECK_GE(resource.length(), ftl_resource_prefix_length);
  *registration_id = resource.substr(ftl_resource_prefix_length);
  return true;
}

bool SignalingAddress::GetFtlSenderEmail(std::string* email) const {
  std::string unused_registration_id;
  return GetFtlInfo(email, &unused_registration_id);
}

}  // namespace remoting
