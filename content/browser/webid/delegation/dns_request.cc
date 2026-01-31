// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/dns_request.h"

#include "base/functional/callback.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "content/browser/webid/delegation/email_verifier_network_request_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "net/base/net_errors.h"
#include "net/dns/public/dns_protocol.h"
#include "url/scheme_host_port.h"

namespace content::webid {

namespace {
void OnDnsResponseParsed(DnsRequest::DnsRequestCallback callback,
                         FetchStatus fetch_status,
                         data_decoder::DataDecoder::ValueOrError result) {
  std::vector<std::string> records;

  bool parse_succeeded = fetch_status.parse_status == ParseStatus::kSuccess;
  if (!parse_succeeded) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const base::DictValue* response = result->GetIfDict();
  if (!response) {
    fetch_status.parse_status = ParseStatus::kInvalidResponseError;
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (response->FindInt("Status").value_or(-1) !=
      net::dns_protocol::kRcodeNOERROR) {
    fetch_status.parse_status = ParseStatus::kInvalidResponseError;
    std::move(callback).Run(std::nullopt);
    return;
  }

  const base::ListValue* answers = response->FindList("Answer");
  if (!answers) {
    fetch_status.parse_status = ParseStatus::kInvalidResponseError;
    std::move(callback).Run(std::nullopt);
    return;
  }

  for (const auto& answer : *answers) {
    const base::DictValue* answer_dict = answer.GetIfDict();
    if (!answer_dict) {
      continue;
    }
    if (answer_dict->FindInt("type").value_or(-1) !=
        net::dns_protocol::kTypeTXT) {
      // Unexpected because we specifically asked for TXT, but ignore this one.
      continue;
    }
    const std::string* data = answer_dict->FindString("data");
    if (data) {
      records.push_back(*data);
    }
  }
  std::move(callback).Run(std::move(records));
}
}  // namespace

DnsRequest::DnsRequest(NetworkRequestManagerGetter network_manager_getter)
    : network_manager_getter_(std::move(network_manager_getter)) {}

DnsRequest::~DnsRequest() = default;

void DnsRequest::SendRequest(const std::string& hostname,
                             DnsRequestCallback callback) {
  EmailVerifierNetworkRequestManager* request_manager =
      network_manager_getter_.Run();
  if (!request_manager) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const std::string& prefix =
      GetContentClient()->browser()->GetDnsTxtResolverUrlPrefix();
  if (prefix.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::string query_url = base::StrCat(
      {prefix, base::EscapeQueryParamValue(hostname, /*use_plus=*/true)});

  request_manager->DownloadAndParseUncredentialedUrl(
      GURL(query_url),
      base::BindOnce(OnDnsResponseParsed, std::move(callback)));
}

}  // namespace content::webid
