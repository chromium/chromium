// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/xmpp_register_support_host_request.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/time/time.h"
#include "remoting/base/constants.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_details.h"
#include "remoting/protocol/errors.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/signaling_id_util.h"
#include "remoting/signaling/xmpp_constants.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlElement;

namespace remoting {

using protocol::ErrorCode;

namespace {
// Strings used in the request message we send to the bot.
const char kRegisterQueryTag[] = "register-support-host";
const char kPublicKeyTag[] = "public-key";
const char kSignatureTag[] = "signature";
const char kSignatureTimeAttr[] = "time";
const char kHostVersionTag[] = "host-version";
const char kHostOsNameTag[] = "host-os-name";
const char kHostOsVersionTag[] = "host-os-version";
const char kAuthorizedHelperTag[] = "authorized-helper";

// Strings used to parse responses received from the bot.
const char kRegisterQueryResultTag[] = "register-support-host-result";
const char kSupportIdTag[] = "support-id";
const char kSupportIdLifetimeTag[] = "support-id-lifetime";

// The signaling timeout for register support host requests.
constexpr int kRegisterRequestTimeoutInSeconds = 10;
}  // namespace

XmppRegisterSupportHostRequest::XmppRegisterSupportHostRequest(
    const std::string& directory_bot_jid)
    : directory_bot_jid_(directory_bot_jid) {}

XmppRegisterSupportHostRequest::~XmppRegisterSupportHostRequest() {
  if (signal_strategy_) {
    signal_strategy_->RemoveListener(this);
  }
}

void XmppRegisterSupportHostRequest::StartRequest(
    SignalStrategy* signal_strategy,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& authorized_helper,
    std::optional<ChromeOsEnterpriseParams> params,
    RegisterCallback callback) {
  DCHECK(signal_strategy);
  DCHECK(key_pair.get());
  DCHECK(callback);
  signal_strategy_ = signal_strategy;
  key_pair_ = key_pair;
  callback_ = std::move(callback);
  signal_strategy_->AddListener(this);
  authorized_helper_ = authorized_helper;
  iq_sender_ = std::make_unique<IqSender>(signal_strategy_);
}

void XmppRegisterSupportHostRequest::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  if (state == SignalStrategy::CONNECTED) {
    DCHECK(!callback_.is_null());
    // The host_jid will be written to the SupportHostStore for lookup. Use id()
    // instead of jid() so that we can write the lcs address instead of the
    // remoting bot JID.
    std::string host_jid = signal_strategy_->GetLocalAddress().id();
    request_ = iq_sender_->SendIq(
        kIqTypeSet, directory_bot_jid_, CreateRegistrationRequest(host_jid),
        base::BindOnce(&XmppRegisterSupportHostRequest::ProcessResponse,
                       base::Unretained(this)));
    if (!request_) {
      LOG(ERROR) << "Error sending the register-support-host request.";
      CallCallback(std::string(), base::TimeDelta(),
                   ErrorCode::SIGNALING_ERROR);
      return;
    }

    request_->SetTimeout(base::Seconds(kRegisterRequestTimeoutInSeconds));

  } else if (state == SignalStrategy::DISCONNECTED) {
    // We will reach here if signaling fails to connect.
    LOG(ERROR) << "Signal strategy disconnected.";
    CallCallback(std::string(), base::TimeDelta(), ErrorCode::SIGNALING_ERROR);
  }
}

bool XmppRegisterSupportHostRequest::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

std::unique_ptr<XmlElement>
XmppRegisterSupportHostRequest::CreateRegistrationRequest(
    const std::string& jid) {
  auto query = std::make_unique<XmlElement>(
      QName(kChromotingXmlNamespace, kRegisterQueryTag));

  auto public_key = std::make_unique<XmlElement>(
      QName(kChromotingXmlNamespace, kPublicKeyTag));
  public_key->AddText(key_pair_->GetPublicKey());
  query->AddElement(public_key.release());

  query->AddElement(CreateSignature(jid).release());

  // Add host version.
  auto host_version = std::make_unique<XmlElement>(
      QName(kChromotingXmlNamespace, kHostVersionTag));
  host_version->AddText(STRINGIZE(VERSION));
  query->AddElement(host_version.release());

  // Add host os name.
  auto host_os_name = std::make_unique<XmlElement>(
      QName(kChromotingXmlNamespace, kHostOsNameTag));
  host_os_name->AddText(GetHostOperatingSystemName());
  query->AddElement(host_os_name.release());

  // Add host os version.
  auto host_os_version = std::make_unique<XmlElement>(
      QName(kChromotingXmlNamespace, kHostOsVersionTag));
  host_os_version->AddText(GetHostOperatingSystemVersion());
  query->AddElement(host_os_version.release());

  // Add authorized helper if one was provided.
  if (!authorized_helper_.empty()) {
    auto authorized_helper = std::make_unique<XmlElement>(
        QName(kChromotingXmlNamespace, kAuthorizedHelperTag));
    authorized_helper->AddText(authorized_helper_);
    query->AddElement(authorized_helper.release());
  }

  return query;
}

std::unique_ptr<XmlElement> XmppRegisterSupportHostRequest::CreateSignature(
    const std::string& jid) {
  std::unique_ptr<XmlElement> signature_tag(
      new XmlElement(QName(kChromotingXmlNamespace, kSignatureTag)));

  int64_t time =
      static_cast<int64_t>(base::Time::Now().InSecondsFSinceUnixEpoch());
  std::string time_str(base::NumberToString(time));
  signature_tag->AddAttr(QName(kChromotingXmlNamespace, kSignatureTimeAttr),
                         time_str);

  std::string message = NormalizeSignalingId(jid) + ' ' + time_str;
  std::string signature(key_pair_->SignMessage(message));
  signature_tag->AddText(signature);

  return signature_tag;
}

void XmppRegisterSupportHostRequest::ParseResponse(const XmlElement* response,
                                                   std::string* support_id,
                                                   base::TimeDelta* lifetime,
                                                   ErrorCode* error_code) {
  if (!response) {
    LOG(ERROR) << "register-support-host request timed out.";
    *error_code = ErrorCode::SIGNALING_TIMEOUT;
    return;
  }

  std::string type = response->Attr(kQNameType);
  if (type == kIqTypeError) {
    LOG(ERROR) << "Received error in response to heartbeat: "
               << response->Str();
    *error_code = ErrorCode::HOST_REGISTRATION_ERROR;
    return;
  }

  // This method must only be called for error or result stanzas.
  if (type != kIqTypeResult) {
    LOG(ERROR) << "Received unexpect stanza of type \"" << type << "\"";
    *error_code = ErrorCode::HOST_REGISTRATION_ERROR;
    return;
  }

  const XmlElement* result_element = response->FirstNamed(
      QName(kChromotingXmlNamespace, kRegisterQueryResultTag));
  if (!result_element) {
    LOG(ERROR) << "<" << kRegisterQueryResultTag
               << "> is missing in the host registration response: "
               << response->Str();
    *error_code = ErrorCode::HOST_REGISTRATION_ERROR;
    return;
  }

  const XmlElement* support_id_element =
      result_element->FirstNamed(QName(kChromotingXmlNamespace, kSupportIdTag));
  if (!support_id_element) {
    LOG(ERROR) << "<" << kSupportIdTag
               << "> is missing in the host registration response: "
               << response->Str();
    *error_code = ErrorCode::HOST_REGISTRATION_ERROR;
    return;
  }

  const XmlElement* lifetime_element = result_element->FirstNamed(
      QName(kChromotingXmlNamespace, kSupportIdLifetimeTag));
  if (!lifetime_element) {
    LOG(ERROR) << "<" << kSupportIdLifetimeTag
               << "> is missing in the host registration response: "
               << response->Str();
    *error_code = ErrorCode::HOST_REGISTRATION_ERROR;
    return;
  }

  int lifetime_int;
  if (!base::StringToInt(lifetime_element->BodyText(), &lifetime_int) ||
      lifetime_int <= 0) {
    LOG(ERROR) << "<" << kSupportIdLifetimeTag
               << "> is malformed in the host registration response: "
               << response->Str();
    *error_code = ErrorCode::HOST_REGISTRATION_ERROR;
    return;
  }

  *support_id = support_id_element->BodyText();
  *lifetime = base::Seconds(lifetime_int);
  return;
}

void XmppRegisterSupportHostRequest::ProcessResponse(
    IqRequest* request,
    const XmlElement* response) {
  std::string support_id;
  base::TimeDelta lifetime;
  ErrorCode error_code = ErrorCode::OK;
  ParseResponse(response, &support_id, &lifetime, &error_code);
  CallCallback(support_id, lifetime, error_code);
}

void XmppRegisterSupportHostRequest::CallCallback(const std::string& support_id,
                                                  base::TimeDelta lifetime,
                                                  ErrorCode error_code) {
  // Cleanup state before calling the callback.
  request_.reset();
  iq_sender_.reset();
  signal_strategy_->RemoveListener(this);
  signal_strategy_ = nullptr;

  std::move(callback_).Run(support_id, lifetime, error_code);
}

}  // namespace remoting
