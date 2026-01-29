// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_message_xml_converter.h"

#include <optional>
#include <string_view>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "remoting/protocol/jingle_messages.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlElement;

namespace remoting::protocol {

namespace {

const char kIceTransportNamespace[] = "google:remoting:ice";
const char kEmptyNamespace[] = "";

const int kPortMin = 1000;
const int kPortMax = 65535;

// The type names "local" and "stun" are not standard but JingleMessage has
// used them from the start. So in order to remain backwards compatible,
// we check specifically for those types and override the candidate type name
// in those cases.
std::string_view GetLegacyTypeName(const webrtc::Candidate& c) {
  if (c.is_local()) {
    return "local";
  }
  if (c.is_stun()) {
    return "stun";
  }
  return c.type_name();
}

std::optional<webrtc::IceCandidateType> LegacyTypeNameToCandidateType(
    std::string_view type) {
  if (type == "local" || type == "host") {
    return webrtc::IceCandidateType::kHost;
  }
  if (type == "stun" || type == "srflx") {
    return webrtc::IceCandidateType::kSrflx;
  }
  if (type == "prflx") {
    return webrtc::IceCandidateType::kPrflx;
  }
  if (type == "relay") {
    return webrtc::IceCandidateType::kRelay;
  }
  return std::nullopt;
}

bool ParseIceCredentials(const jingle_xmpp::XmlElement* element,
                         IceTransportInfo::IceCredentials* credentials) {
  DCHECK(element->Name() == QName(kIceTransportNamespace, "credentials"));

  const std::string& channel = element->Attr(QName(kEmptyNamespace, "channel"));
  const std::string& ufrag = element->Attr(QName(kEmptyNamespace, "ufrag"));
  const std::string& password =
      element->Attr(QName(kEmptyNamespace, "password"));

  if (channel.empty() || ufrag.empty() || password.empty()) {
    return false;
  }

  credentials->channel = channel;
  credentials->ufrag = ufrag;
  credentials->password = password;

  return true;
}

bool ParseIceCandidate(const jingle_xmpp::XmlElement* element,
                       IceTransportInfo::NamedCandidate* candidate) {
  DCHECK(element->Name() == QName(kIceTransportNamespace, "candidate"));

  const std::string& name = element->Attr(QName(kEmptyNamespace, "name"));
  const std::string& foundation =
      element->Attr(QName(kEmptyNamespace, "foundation"));
  const std::string& address = element->Attr(QName(kEmptyNamespace, "address"));
  const std::string& port_str = element->Attr(QName(kEmptyNamespace, "port"));
  const std::optional<webrtc::IceCandidateType> type =
      LegacyTypeNameToCandidateType(
          element->Attr(QName(kEmptyNamespace, "type")));
  const std::string& protocol =
      element->Attr(QName(kEmptyNamespace, "protocol"));
  const std::string& priority_str =
      element->Attr(QName(kEmptyNamespace, "priority"));
  const std::string& generation_str =
      element->Attr(QName(kEmptyNamespace, "generation"));

  int port;
  unsigned priority;
  uint32_t generation;
  if (name.empty() || foundation.empty() || address.empty() ||
      !base::StringToInt(port_str, &port) || port < kPortMin ||
      port > kPortMax || !type || protocol.empty() ||
      !base::StringToUint(priority_str, &priority) ||
      !base::StringToUint(generation_str, &generation)) {
    return false;
  }

  candidate->name = name;
  candidate->candidate = {candidate->candidate.component(),
                          protocol,
                          webrtc::SocketAddress(address, port),
                          priority,
                          candidate->candidate.username(),
                          candidate->candidate.password(),
                          *type,
                          generation,
                          foundation};

  return true;
}

XmlElement* FormatIceCredentials(
    const IceTransportInfo::IceCredentials& credentials) {
  XmlElement* result =
      new XmlElement(QName(kIceTransportNamespace, "credentials"));
  result->SetAttr(QName(kEmptyNamespace, "channel"), credentials.channel);
  result->SetAttr(QName(kEmptyNamespace, "ufrag"), credentials.ufrag);
  result->SetAttr(QName(kEmptyNamespace, "password"), credentials.password);
  return result;
}

XmlElement* FormatIceCandidate(
    const IceTransportInfo::NamedCandidate& candidate) {
  XmlElement* result =
      new XmlElement(QName(kIceTransportNamespace, "candidate"));
  result->SetAttr(QName(kEmptyNamespace, "name"), candidate.name);
  result->SetAttr(QName(kEmptyNamespace, "foundation"),
                  candidate.candidate.foundation());
  result->SetAttr(QName(kEmptyNamespace, "address"),
                  candidate.candidate.address().ipaddr().ToString());
  result->SetAttr(QName(kEmptyNamespace, "port"),
                  base::NumberToString(candidate.candidate.address().port()));
  result->SetAttr(QName(kEmptyNamespace, "type"),
                  GetLegacyTypeName(candidate.candidate));
  result->SetAttr(QName(kEmptyNamespace, "protocol"),
                  candidate.candidate.protocol());
  result->SetAttr(QName(kEmptyNamespace, "priority"),
                  base::NumberToString(candidate.candidate.priority()));
  result->SetAttr(QName(kEmptyNamespace, "generation"),
                  base::NumberToString(candidate.candidate.generation()));
  return result;
}

}  // namespace

std::unique_ptr<jingle_xmpp::XmlElement> IceTransportInfoToXml(
    const IceTransportInfo& transport) {
  std::unique_ptr<jingle_xmpp::XmlElement> result(
      new XmlElement(QName(kIceTransportNamespace, "transport"), true));
  for (const auto& credentials : transport.ice_credentials) {
    result->AddElement(FormatIceCredentials(credentials));
  }
  for (const auto& candidate : transport.candidates) {
    result->AddElement(FormatIceCandidate(candidate));
  }
  return result;
}

bool IceTransportInfoFromXml(const jingle_xmpp::XmlElement* element,
                             IceTransportInfo* transport) {
  if (element->Name() != QName(kIceTransportNamespace, "transport")) {
    return false;
  }

  transport->ice_credentials.clear();
  transport->candidates.clear();

  QName qn_credentials(kIceTransportNamespace, "credentials");
  for (const XmlElement* credentials_tag = element->FirstNamed(qn_credentials);
       credentials_tag;
       credentials_tag = credentials_tag->NextNamed(qn_credentials)) {
    IceTransportInfo::IceCredentials credentials;
    if (!ParseIceCredentials(credentials_tag, &credentials)) {
      return false;
    }
    transport->ice_credentials.push_back(credentials);
  }

  QName qn_candidate(kIceTransportNamespace, "candidate");
  for (const XmlElement* candidate_tag = element->FirstNamed(qn_candidate);
       candidate_tag; candidate_tag = candidate_tag->NextNamed(qn_candidate)) {
    IceTransportInfo::NamedCandidate candidate;
    if (!ParseIceCandidate(candidate_tag, &candidate)) {
      return false;
    }
    transport->candidates.push_back(candidate);
  }

  return true;
}

}  // namespace remoting::protocol
