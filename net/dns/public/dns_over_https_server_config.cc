// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/dns_over_https_server_config.h"

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "net/third_party/uri_template/uri_template.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"
#include "url/url_constants.h"

namespace {

std::optional<std::string> GetHttpsHost(const std::string& url) {
  // This code is used to compute a static initializer, so it runs before GURL's
  // scheme registry is initialized.  Since GURL is not ready yet, we need to
  // duplicate some of its functionality here.
  std::string canonical;
  url::StdStringCanonOutput output(&canonical);
  url::Parsed canonical_parsed;
  bool is_valid =
      url::CanonicalizeStandardURL(url.data(), url::ParseStandardURL(url),
                                   url::SchemeType::SCHEME_WITH_HOST_AND_PORT,
                                   nullptr, &output, &canonical_parsed);
  if (!is_valid)
    return std::nullopt;
  const url::Component& scheme_range = canonical_parsed.scheme;
  std::string_view scheme =
      std::string_view(canonical).substr(scheme_range.begin, scheme_range.len);
  if (scheme != url::kHttpsScheme)
    return std::nullopt;
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
  std::optional<std::string> host = GetHttpsHost(url_string);
  if (!host) {
    // The expanded template must be a valid HTTPS URL.
    return false;
  }
  if (host->find(test_query) != std::string::npos) {
    // The dns variable must not be part of the hostname.
    return false;
  }
  // If the template contains a dns variable, use GET, otherwise use POST.
  *use_post = !base::Contains(vars_found, "dns");
  return true;
}

constexpr std::string_view kJsonKeyTemplate("template");
constexpr std::string_view kJsonKeyEndpoints("endpoints");
constexpr std::string_view kJsonKeyIps("ips");

}  // namespace

namespace net {

DnsOverHttpsServerConfig::DnsOverHttpsServerConfig(std::string server_template,
                                                   bool use_post,
                                                   Endpoints endpoints)
    : server_template_(std::move(server_template)),
      use_post_(use_post),
      endpoints_(std::move(endpoints)) {}

DnsOverHttpsServerConfig::DnsOverHttpsServerConfig() = default;
DnsOverHttpsServerConfig::DnsOverHttpsServerConfig(
    const DnsOverHttpsServerConfig& other) = default;
DnsOverHttpsServerConfig& DnsOverHttpsServerConfig::operator=(
    const DnsOverHttpsServerConfig& other) = default;
DnsOverHttpsServerConfig::DnsOverHttpsServerConfig(
    DnsOverHttpsServerConfig&& other) = default;
DnsOverHttpsServerConfig& DnsOverHttpsServerConfig::operator=(
    DnsOverHttpsServerConfig&& other) = default;

DnsOverHttpsServerConfig::~DnsOverHttpsServerConfig() = default;

std::optional<DnsOverHttpsServerConfig> DnsOverHttpsServerConfig::FromString(
    std::string doh_template,
    Endpoints bindings) {
  bool use_post;
  if (!IsValidDohTemplate(doh_template, &use_post))
    return std::nullopt;
  return DnsOverHttpsServerConfig(std::move(doh_template), use_post,
                                  std::move(bindings));
}

bool DnsOverHttpsServerConfig::operator==(
    const DnsOverHttpsServerConfig& other) const {
  // use_post_ is derived from server_template_, so we don't need to compare it.
  return server_template_ == other.server_template_ &&
         endpoints_ == other.endpoints_;
}

bool DnsOverHttpsServerConfig::operator<(
    const DnsOverHttpsServerConfig& other) const {
  return std::tie(server_template_, endpoints_) <
         std::tie(other.server_template_, other.endpoints_);
}

const std::string& DnsOverHttpsServerConfig::server_template() const {
  return server_template_;
}

std::string_view DnsOverHttpsServerConfig::server_template_piece() const {
  return server_template_;
}

bool DnsOverHttpsServerConfig::use_post() const {
  return use_post_;
}

const DnsOverHttpsServerConfig::Endpoints& DnsOverHttpsServerConfig::endpoints()
    const {
  return endpoints_;
}

bool DnsOverHttpsServerConfig::IsSimple() const {
  return endpoints_.empty();
}

base::Value::Dict DnsOverHttpsServerConfig::ToValue() const {
  base::Value::Dict value;
  value.Set(kJsonKeyTemplate, server_template());
  if (!endpoints_.empty()) {
    base::Value::List bindings;
    bindings.reserve(endpoints_.size());
    for (const IPAddressList& ip_list : endpoints_) {
      base::Value::Dict binding;
      base::Value::List ips;
      ips.reserve(ip_list.size());
      for (const IPAddress& ip : ip_list) {
        ips.Append(ip.ToString());
      }
      binding.Set(kJsonKeyIps, std::move(ips));
      bindings.Append(std::move(binding));
    }
    value.Set(kJsonKeyEndpoints, std::move(bindings));
  }
  return value;
}

// static
std::optional<DnsOverHttpsServerConfig> DnsOverHttpsServerConfig::FromValue(
    base::Value::Dict value) {
  std::string* server_template = value.FindString(kJsonKeyTemplate);
  if (!server_template)
    return std::nullopt;
  bool use_post;
  if (!IsValidDohTemplate(*server_template, &use_post))
    return std::nullopt;
  Endpoints endpoints;
  const base::Value* endpoints_json = value.Find(kJsonKeyEndpoints);
  if (endpoints_json) {
    if (!endpoints_json->is_list())
      return std::nullopt;
    const base::Value::List& json_list = endpoints_json->GetList();
    endpoints.reserve(json_list.size());
    for (const base::Value& endpoint : json_list) {
      const base::Value::Dict* dict = endpoint.GetIfDict();
      if (!dict)
        return std::nullopt;
      IPAddressList parsed_ips;
      const base::Value* ips = dict->Find(kJsonKeyIps);
      if (ips) {
        const base::Value::List* ip_list = ips->GetIfList();
        if (!ip_list)
          return std::nullopt;
        parsed_ips.reserve(ip_list->size());
        for (const base::Value& ip : *ip_list) {
          const std::string* ip_str = ip.GetIfString();
          if (!ip_str)
            return std::nullopt;
          IPAddress parsed;
          if (!parsed.AssignFromIPLiteral(*ip_str))
            return std::nullopt;
          parsed_ips.push_back(std::move(parsed));
        }
      }
      endpoints.push_back(std::move(parsed_ips));
    }
  }
  return DnsOverHttpsServerConfig(std::move(*server_template), use_post,
                                  std::move(endpoints));
}

}  // namespace net
