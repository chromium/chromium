// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_mint_token_flow.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

const char kForceValueFalse[] = "false";
const char kForceValueTrue[] = "true";
const char kResponseTypeValueNone[] = "none";
const char kResponseTypeValueToken[] = "token";

const char kOAuth2IssueTokenBodyFormat[] =
    "force=%s"
    "&response_type=%s"
    "&scope=%s"
    "&client_id=%s"
    "&origin=%s";
// TODO(pavely): lib_ver is passed to differentiate IssueToken requests from
// different code locations. Remove once device_id mismatch is understood.
// (crbug.com/481596)
const char kOAuth2IssueTokenBodyFormatDeviceIdAddendum[] =
    "&device_id=%s&device_type=chrome&lib_ver=extension";
const char kIssueAdviceKey[] = "issueAdvice";
const char kIssueAdviceValueConsent[] = "consent";
const char kAccessTokenKey[] = "token";
const char kConsentKey[] = "consent";
const char kExpiresInKey[] = "expiresIn";
const char kScopesKey[] = "scopes";
const char kDescriptionKey[] = "description";
const char kDetailKey[] = "detail";
const char kDetailSeparators[] = "\n";
const char kError[] = "error";
const char kMessage[] = "message";

static GoogleServiceAuthError CreateAuthError(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  if (net_error == net::ERR_ABORTED) {
    return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
  }
  if (net_error != net::OK) {
    DLOG(WARNING) << "Server returned error: errno " << net_error;
    return GoogleServiceAuthError::FromConnectionError(net_error);
  }

  std::string response_body;
  if (body)
    response_body = std::move(*body);

  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadDeprecated(response_body);
  base::DictionaryValue* response;
  if (!value.get() || !value->GetAsDictionary(&response)) {
    int http_response_code = -1;
    if (head && head->headers)
      http_response_code = head->headers->response_code();
    return GoogleServiceAuthError::FromUnexpectedServiceResponse(
        base::StringPrintf("Not able to parse a JSON object from "
                           "a service response. "
                           "HTTP Status of the response is: %d",
                           http_response_code));
  }
  base::DictionaryValue* error;
  if (!response->GetDictionary(kError, &error)) {
    return GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to find a detailed error in a service response.");
  }
  std::string message;
  if (!error->GetString(kMessage, &message)) {
    return GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to find an error message within a service error.");
  }
  return GoogleServiceAuthError::FromServiceError(message);
}

}  // namespace

IssueAdviceInfoEntry::IssueAdviceInfoEntry() {}
IssueAdviceInfoEntry::IssueAdviceInfoEntry(const IssueAdviceInfoEntry& other) =
    default;
IssueAdviceInfoEntry::~IssueAdviceInfoEntry() {}

bool IssueAdviceInfoEntry::operator ==(const IssueAdviceInfoEntry& rhs) const {
  return description == rhs.description && details == rhs.details;
}

OAuth2MintTokenFlow::Parameters::Parameters() : mode(MODE_ISSUE_ADVICE) {}

OAuth2MintTokenFlow::Parameters::Parameters(
    const std::string& eid,
    const std::string& cid,
    const std::vector<std::string>& scopes_arg,
    const std::string& device_id,
    Mode mode_arg)
    : extension_id(eid),
      client_id(cid),
      scopes(scopes_arg),
      device_id(device_id),
      mode(mode_arg) {
}

OAuth2MintTokenFlow::Parameters::Parameters(const Parameters& other) = default;

OAuth2MintTokenFlow::Parameters::~Parameters() {}

OAuth2MintTokenFlow::OAuth2MintTokenFlow(Delegate* delegate,
                                         const Parameters& parameters)
    : delegate_(delegate), parameters_(parameters) {}

OAuth2MintTokenFlow::~OAuth2MintTokenFlow() { }

void OAuth2MintTokenFlow::ReportSuccess(const std::string& access_token,
                                        int time_to_live) {
  if (delegate_)
    delegate_->OnMintTokenSuccess(access_token, time_to_live);

  // |this| may already be deleted.
}

void OAuth2MintTokenFlow::ReportIssueAdviceSuccess(
    const IssueAdviceInfo& issue_advice) {
  if (delegate_)
    delegate_->OnIssueAdviceSuccess(issue_advice);

  // |this| may already be deleted.
}

void OAuth2MintTokenFlow::ReportFailure(
    const GoogleServiceAuthError& error) {
  if (delegate_)
    delegate_->OnMintTokenFailure(error);

  // |this| may already be deleted.
}

GURL OAuth2MintTokenFlow::CreateApiCallUrl() {
  return GaiaUrls::GetInstance()->oauth2_issue_token_url();
}

std::string OAuth2MintTokenFlow::CreateApiCallBody() {
  const char* force_value =
      (parameters_.mode == MODE_MINT_TOKEN_FORCE ||
       parameters_.mode == MODE_RECORD_GRANT)
          ? kForceValueTrue : kForceValueFalse;
  const char* response_type_value =
      (parameters_.mode == MODE_MINT_TOKEN_NO_FORCE ||
       parameters_.mode == MODE_MINT_TOKEN_FORCE)
          ? kResponseTypeValueToken : kResponseTypeValueNone;
  std::string body = base::StringPrintf(
      kOAuth2IssueTokenBodyFormat,
      net::EscapeUrlEncodedData(force_value, true).c_str(),
      net::EscapeUrlEncodedData(response_type_value, true).c_str(),
      net::EscapeUrlEncodedData(
          base::JoinString(parameters_.scopes, " "), true).c_str(),
      net::EscapeUrlEncodedData(parameters_.client_id, true).c_str(),
      net::EscapeUrlEncodedData(parameters_.extension_id, true).c_str());
  if (!parameters_.device_id.empty()) {
    body.append(base::StringPrintf(
        kOAuth2IssueTokenBodyFormatDeviceIdAddendum,
        net::EscapeUrlEncodedData(parameters_.device_id, true).c_str()));
  }
  return body;
}

void OAuth2MintTokenFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  std::string response_body;
  if (body)
    response_body = std::move(*body);
  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadDeprecated(response_body);
  base::DictionaryValue* dict = nullptr;
  if (!value.get() || !value->GetAsDictionary(&dict)) {
    ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to parse a JSON object from a service response."));
    return;
  }

  std::string issue_advice_value;
  if (!dict->GetString(kIssueAdviceKey, &issue_advice_value)) {
    ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to find an issueAdvice in a service response."));
    return;
  }
  if (issue_advice_value == kIssueAdviceValueConsent) {
    IssueAdviceInfo issue_advice;
    if (ParseIssueAdviceResponse(dict, &issue_advice))
      ReportIssueAdviceSuccess(issue_advice);
    else
      ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
          "Not able to parse the contents of consent "
          "from a service response."));
  } else {
    std::string access_token;
    int time_to_live;
    if (ParseMintTokenResponse(dict, &access_token, &time_to_live))
      ReportSuccess(access_token, time_to_live);
    else
      ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
          "Not able to parse the contents of access token "
          "from a service response."));
  }

  // |this| may be deleted!
}

void OAuth2MintTokenFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  ReportFailure(CreateAuthError(net_error, head, std::move(body)));
}

// static
bool OAuth2MintTokenFlow::ParseMintTokenResponse(
    const base::DictionaryValue* dict, std::string* access_token,
    int* time_to_live) {
  CHECK(dict);
  CHECK(access_token);
  CHECK(time_to_live);
  std::string ttl_string;
  return dict->GetString(kExpiresInKey, &ttl_string) &&
      base::StringToInt(ttl_string, time_to_live) &&
      dict->GetString(kAccessTokenKey, access_token);
}

// static
bool OAuth2MintTokenFlow::ParseIssueAdviceResponse(
    const base::DictionaryValue* dict, IssueAdviceInfo* issue_advice) {
  CHECK(dict);
  CHECK(issue_advice);

  const base::DictionaryValue* consent_dict = nullptr;
  if (!dict->GetDictionary(kConsentKey, &consent_dict))
    return false;

  const base::ListValue* scopes_list = nullptr;
  if (!consent_dict->GetList(kScopesKey, &scopes_list))
    return false;

  bool success = true;
  for (size_t index = 0; index < scopes_list->GetSize(); ++index) {
    const base::DictionaryValue* scopes_entry = nullptr;
    IssueAdviceInfoEntry entry;
    base::string16 detail;
    if (!scopes_list->GetDictionary(index, &scopes_entry) ||
        !scopes_entry->GetString(kDescriptionKey, &entry.description) ||
        !scopes_entry->GetString(kDetailKey, &detail)) {
      success = false;
      break;
    }

    base::TrimWhitespace(entry.description, base::TRIM_ALL, &entry.description);
    entry.details = base::SplitString(
        detail, base::ASCIIToUTF16(kDetailSeparators),
        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    issue_advice->push_back(entry);
  }

  if (!success)
    issue_advice->clear();

  return success;
}

net::PartialNetworkTrafficAnnotationTag
OAuth2MintTokenFlow::GetNetworkTrafficAnnotationTag() {
  return net::DefinePartialNetworkTrafficAnnotation(
      "oauth2_mint_token_flow", "oauth2_api_call_flow", R"(
      semantics {
        sender: "Chrome Identity API"
        description:
          "Requests a token from gaia allowing an app or extension to act as "
          "the user when calling other google APIs."
        trigger: "API call from the app/extension."
        data:
          "User's login token, the identity of a chrome app/extension, and a "
          "list of oauth scopes requested by the app/extension."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        setting:
          "This feature cannot be disabled by settings, however the request is "
          "made only for signed-in users."
        chrome_policy {
          SigninAllowed {
            policy_options {mode: MANDATORY}
            SigninAllowed: false
          }
        }
      })");
}
