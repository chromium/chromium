// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/gaia_token_pre_xmpp_auth.h"

#include <algorithm>

#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"
#include "third_party/libjingle_xmpp/xmpp/saslcookiemechanism.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace notifier {

namespace {

class GaiaCookieMechanism : public jingle_xmpp::SaslCookieMechanism {
 public:
  GaiaCookieMechanism(const std::string & mechanism,
                      const std::string & username,
                      const std::string & cookie,
                      const std::string & token_service)
      : jingle_xmpp::SaslCookieMechanism(
          mechanism, username, cookie, token_service) {}

  ~GaiaCookieMechanism() override {}

  jingle_xmpp::XmlElement* StartSaslAuth() override {
    jingle_xmpp::XmlElement* auth = jingle_xmpp::SaslCookieMechanism::StartSaslAuth();
    // These attributes are necessary for working with non-gmail gaia
    // accounts.
    const std::string NS_GOOGLE_AUTH_PROTOCOL(
        "http://www.google.com/talk/protocol/auth");
    const jingle_xmpp::QName QN_GOOGLE_ALLOW_GENERATED_JID_XMPP_LOGIN(
        NS_GOOGLE_AUTH_PROTOCOL, "allow-generated-jid");
    const jingle_xmpp::QName QN_GOOGLE_AUTH_CLIENT_USES_FULL_BIND_RESULT(
        NS_GOOGLE_AUTH_PROTOCOL, "client-uses-full-bind-result");
    auth->SetAttr(QN_GOOGLE_ALLOW_GENERATED_JID_XMPP_LOGIN, "true");
    auth->SetAttr(QN_GOOGLE_AUTH_CLIENT_USES_FULL_BIND_RESULT, "true");
    return auth;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GaiaCookieMechanism);
};

}  // namespace

GaiaTokenPreXmppAuth::GaiaTokenPreXmppAuth(
    const std::string& username,
    const std::string& token,
    const std::string& token_service,
    const std::string& auth_mechanism)
    : username_(username),
      token_(token),
      token_service_(token_service),
      auth_mechanism_(auth_mechanism) {
  DCHECK(!auth_mechanism_.empty());
}

GaiaTokenPreXmppAuth::~GaiaTokenPreXmppAuth() { }

void GaiaTokenPreXmppAuth::StartPreXmppAuth(
    const jingle_xmpp::Jid& jid,
    const std::string& pass,
    const std::string& auth_mechanism,
    const std::string& auth_token) {
  SignalAuthDone();
}

bool GaiaTokenPreXmppAuth::IsAuthDone() const {
  return true;
}

bool GaiaTokenPreXmppAuth::IsAuthorized() const {
  return true;
}

bool GaiaTokenPreXmppAuth::HadError() const {
  return false;
}

int GaiaTokenPreXmppAuth::GetError() const {
  return 0;
}

jingle_xmpp::CaptchaChallenge GaiaTokenPreXmppAuth::GetCaptchaChallenge() const {
  return jingle_xmpp::CaptchaChallenge();
}

std::string GaiaTokenPreXmppAuth::GetAuthToken() const {
  return token_;
}

std::string GaiaTokenPreXmppAuth::GetAuthMechanism() const {
  return auth_mechanism_;
}

std::string GaiaTokenPreXmppAuth::ChooseBestSaslMechanism(
    const std::vector<std::string> & mechanisms, bool encrypted) {
  return base::Contains(mechanisms, auth_mechanism_) ? auth_mechanism_
                                                     : std::string();
}

jingle_xmpp::SaslMechanism* GaiaTokenPreXmppAuth::CreateSaslMechanism(
    const std::string& mechanism) {
  if (mechanism == auth_mechanism_)
    return new GaiaCookieMechanism(
        mechanism, username_, token_, token_service_);
  return NULL;
}

}  // namespace notifier
