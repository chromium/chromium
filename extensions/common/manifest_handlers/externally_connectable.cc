// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/externally_connectable.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/api/extensions_manifest_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/url_pattern.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace rcd = net::registry_controlled_domains;

namespace extensions {

namespace externally_connectable_errors {
const char kErrorInvalidMatchPattern[] = "Invalid match pattern '*'";
const char kErrorInvalidId[] = "Invalid ID '*'";
const char kErrorNothingSpecified[] =
    "'externally_connectable' specifies neither 'matches' nor 'ids'; "
    "nothing will be able to connect";
const char kErrorTopLevelDomainsNotAllowed[] =
    "\"*\" is an effective top level domain for which wildcard subdomains such "
    "as \"*\" are not allowed";
const char kErrorWildcardHostsNotAllowed[] =
    "Wildcard domain patterns such as \"*\" are not allowed";
}  // namespace externally_connectable_errors

namespace keys = extensions::manifest_keys;
using api::extensions_manifest_types::ExternallyConnectable;

namespace {

const char kAllIds[] = "*";

template <typename T>
std::vector<T> Sorted(const std::vector<T>& in) {
  std::vector<T> out = in;
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace

ExternallyConnectableHandler::ExternallyConnectableHandler() {
}

ExternallyConnectableHandler::~ExternallyConnectableHandler() {
}

bool ExternallyConnectableHandler::Parse(Extension* extension,
                                         base::string16* error) {
  const base::Value* externally_connectable = NULL;
  CHECK(extension->manifest()->Get(keys::kExternallyConnectable,
                                   &externally_connectable));
  bool allow_all_urls = PermissionsParser::HasAPIPermission(
      extension, APIPermission::kExternallyConnectableAllUrls);

  std::vector<InstallWarning> install_warnings;
  std::unique_ptr<ExternallyConnectableInfo> info =
      ExternallyConnectableInfo::FromValue(
          *externally_connectable, allow_all_urls, &install_warnings, error);
  if (!info)
    return false;
  if (!info->matches.is_empty()) {
    PermissionsParser::AddAPIPermission(extension,
                                        APIPermission::kWebConnectable);
  }
  extension->AddInstallWarnings(std::move(install_warnings));
  extension->SetManifestData(keys::kExternallyConnectable, std::move(info));
  return true;
}

base::span<const char* const> ExternallyConnectableHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kExternallyConnectable};
  return kKeys;
}

// static
ExternallyConnectableInfo* ExternallyConnectableInfo::Get(
    const Extension* extension) {
  return static_cast<ExternallyConnectableInfo*>(
      extension->GetManifestData(keys::kExternallyConnectable));
}

// static
std::unique_ptr<ExternallyConnectableInfo> ExternallyConnectableInfo::FromValue(
    const base::Value& value,
    bool allow_all_urls,
    std::vector<InstallWarning>* install_warnings,
    base::string16* error) {
  std::unique_ptr<ExternallyConnectable> externally_connectable =
      ExternallyConnectable::FromValue(value, error);
  if (!externally_connectable)
    return std::unique_ptr<ExternallyConnectableInfo>();

  URLPatternSet matches;

  if (externally_connectable->matches) {
    for (auto it = externally_connectable->matches->begin();
         it != externally_connectable->matches->end(); ++it) {
      // Safe to use SCHEME_ALL here; externally_connectable gives a page ->
      // extension communication path, not the other way.
      URLPattern pattern(URLPattern::SCHEME_ALL);
      if (pattern.Parse(*it) != URLPattern::ParseResult::kSuccess) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            externally_connectable_errors::kErrorInvalidMatchPattern, *it);
        return std::unique_ptr<ExternallyConnectableInfo>();
      }

      bool matches_all_hosts =
          pattern.match_all_urls() ||  // <all_urls>
          (pattern.host().empty() &&
           pattern.match_subdomains());  // e.g., https://*/*

      if (allow_all_urls && matches_all_hosts) {
        matches.AddPattern(pattern);
        continue;
      }

      // Wildcard hosts are not allowed.
      if (pattern.host().empty()) {
        // Warning not error for forwards compatibility.
        install_warnings->push_back(InstallWarning(
            ErrorUtils::FormatErrorMessage(
                externally_connectable_errors::kErrorWildcardHostsNotAllowed,
                *it),
            keys::kExternallyConnectable, *it));
        continue;
      }

      // Broad match patterns like "*.com", "*.co.uk", and even "*.appspot.com"
      // are not allowed. However just "appspot.com" is ok.
      if (pattern.MatchesEffectiveTld(rcd::INCLUDE_PRIVATE_REGISTRIES,
                                      rcd::INCLUDE_UNKNOWN_REGISTRIES)) {
        // Warning not error for forwards compatibility.
        install_warnings->push_back(InstallWarning(
            ErrorUtils::FormatErrorMessage(
                externally_connectable_errors::kErrorTopLevelDomainsNotAllowed,
                pattern.host().c_str(), *it),
            keys::kExternallyConnectable, *it));
        continue;
      }

      matches.AddPattern(pattern);
    }
  }

  std::vector<std::string> ids;
  bool all_ids = false;

  if (externally_connectable->ids) {
    for (auto it = externally_connectable->ids->begin();
         it != externally_connectable->ids->end(); ++it) {
      if (*it == kAllIds) {
        all_ids = true;
      } else if (crx_file::id_util::IdIsValid(*it)) {
        ids.push_back(*it);
      } else {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            externally_connectable_errors::kErrorInvalidId, *it);
        return std::unique_ptr<ExternallyConnectableInfo>();
      }
    }
  }

  if (!externally_connectable->matches && !externally_connectable->ids) {
    install_warnings->push_back(
        InstallWarning(externally_connectable_errors::kErrorNothingSpecified,
                       keys::kExternallyConnectable));
  }

  bool accepts_tls_channel_id =
      externally_connectable->accepts_tls_channel_id.get() &&
      *externally_connectable->accepts_tls_channel_id;
  return base::WrapUnique(new ExternallyConnectableInfo(
      std::move(matches), ids, all_ids, accepts_tls_channel_id));
}

ExternallyConnectableInfo::~ExternallyConnectableInfo() {
}

ExternallyConnectableInfo::ExternallyConnectableInfo(
    URLPatternSet matches,
    const std::vector<std::string>& ids,
    bool all_ids,
    bool accepts_tls_channel_id)
    : matches(std::move(matches)),
      ids(Sorted(ids)),
      all_ids(all_ids),
      accepts_tls_channel_id(accepts_tls_channel_id) {
}

bool ExternallyConnectableInfo::IdCanConnect(const std::string& id) {
  if (all_ids)
    return true;
  DCHECK(base::STLIsSorted(ids));
  return std::binary_search(ids.begin(), ids.end(), id);
}

}  // namespace extensions
