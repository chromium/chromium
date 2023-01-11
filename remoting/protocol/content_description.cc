// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/content_description.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/base/name_value_map.h"
#include "remoting/protocol/authenticator.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlElement;

namespace remoting::protocol {

const char ContentDescription::kChromotingContentName[] = "chromoting";

namespace {

const char kDefaultNs[] = "";

// Following constants are used to format session description in XML.
const char kDescriptionTag[] = "description";
const char kStandardIceTag[] = "standard-ice";
const char kControlTag[] = "control";
const char kEventTag[] = "event";
const char kVideoTag[] = "video";
const char kAudioTag[] = "audio";

const char kTransportAttr[] = "transport";
const char kVersionAttr[] = "version";
const char kCodecAttr[] = "codec";

const NameMapElement<ChannelConfig::TransportType> kTransports[] = {
    {ChannelConfig::TRANSPORT_STREAM, "stream"},
    {ChannelConfig::TRANSPORT_MUX_STREAM, "mux-stream"},
    {ChannelConfig::TRANSPORT_DATAGRAM, "datagram"},
    {ChannelConfig::TRANSPORT_NONE, "none"},
};

const NameMapElement<ChannelConfig::Codec> kCodecs[] = {
    {ChannelConfig::CODEC_VERBATIM, "verbatim"},
    {ChannelConfig::CODEC_VP8, "vp8"},
    {ChannelConfig::CODEC_VP9, "vp9"},
    {ChannelConfig::CODEC_H264, "h264"},
    {ChannelConfig::CODEC_ZIP, "zip"},
    {ChannelConfig::CODEC_OPUS, "opus"},
    {ChannelConfig::CODEC_SPEEX, "speex"},
    {ChannelConfig::CODEC_AV1, "av1"},
};

// Format a channel configuration tag for chromotocol session description,
// e.g. for video channel:
//    <video transport="stream" version="1" codec="vp8" />
XmlElement* FormatChannelConfig(const ChannelConfig& config,
                                const std::string& tag_name) {
  XmlElement* result = new XmlElement(QName(kChromotingXmlNamespace, tag_name));

  result->AddAttr(QName(kDefaultNs, kTransportAttr),
                  ValueToName(kTransports, config.transport));

  if (config.transport != ChannelConfig::TRANSPORT_NONE) {
    result->AddAttr(QName(kDefaultNs, kVersionAttr),
                    base::NumberToString(config.version));

    if (config.codec != ChannelConfig::CODEC_UNDEFINED) {
      result->AddAttr(QName(kDefaultNs, kCodecAttr),
                      ValueToName(kCodecs, config.codec));
    }
  }

  return result;
}

// Returns false if the element is invalid.
bool ParseChannelConfig(const XmlElement* element,
                        bool codec_required,
                        ChannelConfig* config) {
  if (!NameToValue(kTransports,
                   element->Attr(QName(kDefaultNs, kTransportAttr)),
                   &config->transport)) {
    return false;
  }

  // Version is not required when transport="none".
  if (config->transport != ChannelConfig::TRANSPORT_NONE) {
    if (!base::StringToInt(element->Attr(QName(kDefaultNs, kVersionAttr)),
                           &config->version)) {
      return false;
    }

    // Codec is not required when transport="none".
    if (codec_required) {
      if (!NameToValue(kCodecs, element->Attr(QName(kDefaultNs, kCodecAttr)),
                       &config->codec)) {
        return false;
      }
    } else {
      config->codec = ChannelConfig::CODEC_UNDEFINED;
    }
  } else {
    config->version = 0;
    config->codec = ChannelConfig::CODEC_UNDEFINED;
  }

  return true;
}

}  // namespace

ContentDescription::ContentDescription(
    std::unique_ptr<CandidateSessionConfig> config,
    std::unique_ptr<jingle_xmpp::XmlElement> authenticator_message)
    : candidate_config_(std::move(config)),
      authenticator_message_(std::move(authenticator_message)) {}

ContentDescription::~ContentDescription() = default;

// ToXml() creates content description for chromoting session. The
// description looks as follows:
//   <description xmlns="google:remoting">
//     <standard-ice/>
//     <control transport="stream" version="1" />
//     <event transport="datagram" version="1" />
//     <video transport="stream" codec="vp8" version="1" />
//     <audio transport="stream" codec="opus" version="1" />
//     <authentication>
//      Message created by Authenticator implementation.
//     </authentication>
//   </description>
//
XmlElement* ContentDescription::ToXml() const {
  XmlElement* root =
      new XmlElement(QName(kChromotingXmlNamespace, kDescriptionTag), true);

  if (config()->ice_supported()) {
    root->AddElement(new jingle_xmpp::XmlElement(
        QName(kChromotingXmlNamespace, kStandardIceTag)));

    for (const auto& channel_config : config()->control_configs()) {
      root->AddElement(FormatChannelConfig(channel_config, kControlTag));
    }

    for (const auto& channel_config : config()->event_configs()) {
      root->AddElement(FormatChannelConfig(channel_config, kEventTag));
    }

    for (const auto& channel_config : config()->video_configs()) {
      root->AddElement(FormatChannelConfig(channel_config, kVideoTag));
    }

    for (const auto& channel_config : config()->audio_configs()) {
      root->AddElement(FormatChannelConfig(channel_config, kAudioTag));
    }
  }

  if (authenticator_message_) {
    DCHECK(Authenticator::IsAuthenticatorMessage(authenticator_message_.get()));
    root->AddElement(new XmlElement(*authenticator_message_));
  }

  return root;
}

// static
// Adds the channel configs corresponding to |tag_name|, found in |element|, to
// |configs|.
bool ContentDescription::ParseChannelConfigs(
    const XmlElement* const element,
    const char tag_name[],
    bool codec_required,
    bool optional,
    std::list<ChannelConfig>* const configs) {
  QName tag(kChromotingXmlNamespace, tag_name);
  const XmlElement* child = element->FirstNamed(tag);
  while (child) {
    ChannelConfig channel_config;
    if (ParseChannelConfig(child, codec_required, &channel_config)) {
      configs->push_back(channel_config);
    }
    child = child->NextNamed(tag);
  }
  if (optional && configs->empty()) {
    // If there's no mention of the tag, implicitly assume disabled channel.
    configs->push_back(ChannelConfig::None());
  }
  return true;
}

// static
std::unique_ptr<ContentDescription> ContentDescription::ParseXml(
    const XmlElement* element,
    bool webrtc_transport) {
  if (element->Name() != QName(kChromotingXmlNamespace, kDescriptionTag)) {
    LOG(ERROR) << "Invalid description: " << element->Str();
    return nullptr;
  }
  std::unique_ptr<CandidateSessionConfig> config(
      CandidateSessionConfig::CreateEmpty());

  config->set_webrtc_supported(webrtc_transport);

  if (element->FirstNamed(QName(kChromotingXmlNamespace, kStandardIceTag)) !=
      nullptr) {
    config->set_ice_supported(true);
    if (!ParseChannelConfigs(element, kControlTag, false, false,
                             config->mutable_control_configs()) ||
        !ParseChannelConfigs(element, kEventTag, false, false,
                             config->mutable_event_configs()) ||
        !ParseChannelConfigs(element, kVideoTag, true, false,
                             config->mutable_video_configs()) ||
        !ParseChannelConfigs(element, kAudioTag, true, true,
                             config->mutable_audio_configs())) {
      return nullptr;
    }
  }

  std::unique_ptr<XmlElement> authenticator_message;
  const XmlElement* child = Authenticator::FindAuthenticatorMessage(element);
  if (child) {
    authenticator_message = std::make_unique<XmlElement>(*child);
  }

  return base::WrapUnique(new ContentDescription(
      std::move(config), std::move(authenticator_message)));
}

}  // namespace remoting::protocol
