// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/dns_over_https_server_config.h"

#include <set>
#include <string>
#include <unordered_map>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "net/third_party/uri_template/uri_template.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"
#include "url/url_constants.h"

namespace {

absl::optional<std::string> GetHttpsHost(const std::string& url) {
  // This code is used to compute a static initializer, so it runs before GURL's
  // scheme registry is initialized.  Since GURL is not ready yet, we need to
  // duplicate some of its functionality here.
  url::Parsed parsed;
  url::ParseStandardURL(url.data(), url.size(), &parsed);
  std::string canonical;
  url::StdStringCanonOutput output(&canonical);
  url::Parsed canonical_parsed;
  bool is_valid =
      url::CanonicalizeStandardURL(url.data(), url.size(), parsed,
                                   url::SchemeType::SCHEME_WITH_HOST_AND_PORT,
                                   nullptr, &output, &canonical_parsed);
  if (!is_valid)
    return absl::nullopt;
  const url::Component& scheme_range = canonical_parsed.scheme;
  base::StringPiece scheme =
      base::StringPiece(canonical).substr(scheme_range.begin, scheme_range.len);
  if (scheme != url::kHttpsScheme)
    return absl::nullopt;
  const url::Component& host_range = canonical_parsed.host;
  return canonical.substr(host_range.begin, host_range.len);
}

bool IsValidDohTemplate(const std::string& server_template, bool* use_post) {
  std::string url_string;
  std::string test_query = "this_is_a_test_query";
  std::unordered_map<std::string, std::string> template_params(
      {{"dns", test_query}});
  std::set<std::string> vars_found;
  bool valid_template = uri_template::Expand(server_template, template_params,
                                             &url_string, &vars_found);
  if (!valid_template) {
    // The URI template is malformed.
    return false;
  }
  absl::optional<std::string> host = GetHttpsHost(url_string);
  if (!host) {
    // The expanded template must be a valid HTTPS URL.
    return false;
  }
  if (host->find(test_query) != std::string::npos) {
    // The dns variable must not be part of the hostname.
    return false;
  }
  // If the template contains a dns variable, use GET, otherwise use POST.
  *use_post = vars_found.find("dns") == vars_found.end();
  return true;
}

}  // namespace

namespace net {

absl::optional<DnsOverHttpsServerConfig> DnsOverHttpsServerConfig::FromString(
    std::string doh_template) {
  bool use_post;
  if (!IsValidDohTemplate(doh_template, &use_post))
    return absl::nullopt;
  return DnsOverHttpsServerConfig(std::move(doh_template), use_post);
}

bool DnsOverHttpsServerConfig::operator==(
    const DnsOverHttpsServerConfig& other) const {
  // use_post_ is derived from server_template_, so we don't need to compare it.
  return server_template_ == other.server_template_;
}

bool DnsOverHttpsServerConfig::operator<(
    const DnsOverHttpsServerConfig& other) const {
  return server_template_ < other.server_template_;
}

const std::string& DnsOverHttpsServerConfig::server_template() const {
  return server_template_;
}

base::StringPiece DnsOverHttpsServerConfig::server_template_piece() const {
  return server_template_;
}

bool DnsOverHttpsServerConfig::use_post() const {
  return use_post_;
}

base::Value DnsOverHttpsServerConfig::ToValue() const {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey("server_template", server_template());
  return value;
}

}  // namespace net
