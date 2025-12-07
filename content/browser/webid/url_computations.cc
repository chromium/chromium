// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/url_computations.h"

#include <string>

#include "base/containers/contains.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/mappers.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

using RpMode = blink::mojom::RpMode;

namespace content {
namespace webid {

namespace {

bool IsRequestingDefaultPermissions(const std::vector<std::string>& fields) {
  return base::Contains(fields, webid::kDefaultFieldName) &&
         base::Contains(fields, webid::kDefaultFieldEmail) &&
         base::Contains(fields, webid::kDefaultFieldPicture);
}

}  // namespace

std::string ComputeUrlEncodedTokenPostData(
    RenderFrameHost& render_frame_host,
    const std::string& client_id,
    const std::string& nonce,
    const std::string& account_id,
    bool is_auto_reauthn,
    const RpMode& rp_mode,
    const std::optional<std::vector<std::string>>& fields,
    const std::vector<std::string>& disclosure_shown_for,
    const std::string& params_json,
    const std::optional<std::string>& type) {
  std::string query;
  if (!client_id.empty()) {
    query +=
        "client_id=" + base::EscapeUrlEncodedData(client_id, /*use_plus=*/true);
  }

  if (!webid::IsNonceInParamsEnabled() && !nonce.empty()) {
    render_frame_host.AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        "The 'nonce' parameter should be passed within the 'params' "
        "object instead of as a top-level parameter. Top-level nonce "
        "support will be removed in Chrome 145.");

    if (!query.empty()) {
      query += "&";
    }
    query += "nonce=" + base::EscapeUrlEncodedData(nonce, /*use_plus=*/true);
  }

  if (!account_id.empty()) {
    if (!query.empty()) {
      query += "&";
    }
    query += "account_id=" +
             base::EscapeUrlEncodedData(account_id, /*use_plus=*/true);
  }
  // For new users signing up, we show some disclosure text to remind them about
  // data sharing between IDP and RP. For returning users signing in, such
  // disclosure text is not necessary. This field indicates in the request
  // whether the user has been shown such disclosure text.
  std::string disclosure_text_shown_param =
      base::ToString(IsRequestingDefaultPermissions(disclosure_shown_for));
  if (!query.empty()) {
    query += "&";
  }
  query += "disclosure_text_shown=" + disclosure_text_shown_param;

  // Shares with IdP that whether the identity credential was automatically
  // selected. This could help developers to better comprehend the token
  // request and segment metrics accordingly.
  std::string is_auto_selected = base::ToString(is_auto_reauthn);
  if (!query.empty()) {
    query += "&";
  }
  query += "is_auto_selected=" + is_auto_selected;

  // Shares with IdP the type of the request.
  std::string rp_mode_str = rp_mode == RpMode::kActive ? "active" : "passive";
  if (!query.empty()) {
    query += "&";
  }
  query += "mode=" + rp_mode_str;

  std::vector<std::string> fields_to_use;
  if (fields) {
    fields_to_use = *fields;
  } else {
    fields_to_use = {webid::kDefaultFieldName, webid::kDefaultFieldEmail,
                     webid::kDefaultFieldPicture};
  }
  if (!fields_to_use.empty()) {
    query += "&fields=" +
             base::EscapeUrlEncodedData(base::JoinString(fields_to_use, ","),
                                        /*use_plus=*/true);
  }

  if (!disclosure_shown_for.empty()) {
    query +=
        "&disclosure_shown_for=" +
        base::EscapeUrlEncodedData(base::JoinString(disclosure_shown_for, ","),
                                   /*use_plus=*/true);
  }

  if (!params_json.empty()) {
    query +=
        "&params=" + base::EscapeUrlEncodedData(params_json, /*use_plus=*/true);
  }
  if (IsIdPRegistrationEnabled() && type) {
    query += "&type=" + base::EscapeUrlEncodedData(*type, /*use_plus=*/true);
  }
  return query;
}

void MaybeAppendQueryParameters(
    const IdentityProviderLoginUrlInfo& idp_login_info,
    GURL* login_url) {
  if (idp_login_info.login_hint.empty() && idp_login_info.domain_hint.empty()) {
    return;
  }
  std::string old_query = login_url->GetQuery();
  if (!old_query.empty()) {
    old_query += "&";
  }
  std::string new_query_string = old_query;
  if (!idp_login_info.login_hint.empty()) {
    new_query_string +=
        "login_hint=" + base::EscapeUrlEncodedData(idp_login_info.login_hint,
                                                   /*use_plus=*/false);
  }
  if (!idp_login_info.domain_hint.empty()) {
    if (!new_query_string.empty()) {
      new_query_string += "&";
    }
    new_query_string +=
        "domain_hint=" + base::EscapeUrlEncodedData(idp_login_info.domain_hint,
                                                    /*use_plus=*/false);
  }
  GURL::Replacements replacements;
  replacements.SetQueryStr(new_query_string);
  *login_url = login_url->ReplaceComponents(replacements);
}

}  // namespace webid
}  // namespace content
