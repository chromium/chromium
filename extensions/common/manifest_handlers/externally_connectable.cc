// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/externally_connectable.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/api/extensions_manifest_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/url_pattern.h"
#include "url/gurl.h"

namespace extensions {

namespace externally_connectable_errors {
const char kErrorInvalidMatchPattern[] = "Invalid match pattern '*'";
const char kErrorInvalidId[] = "Invalid ID '*'";
const char kErrorNothingSpecified[] =
    "'externally_connectable' specifies neither 'matches' nor 'ids'; "
    "nothing will be able to connect";
const char kErrorUnusedAcceptsTlsChannelId[] =
    "'externally_connectable' specifies 'accepts_tls_channel_id' but not "
    "'matches'; 'accepts_tls_channel_id' has no efect";
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

ExternallyConnectableHandler::ExternallyConnectableHandler() = default;

ExternallyConnectableHandler::~ExternallyConnectableHandler() = default;

bool ExternallyConnectableHandler::Parse(Extension* extension,
                                         std::u16string* error) {
  const base::Value* externally_connectable =
      extension->manifest()->FindPath(keys::kExternallyConnectable);
  CHECK(externally_connectable != nullptr);

  std::vector<InstallWarning> install_warnings;
  std::unique_ptr<ExternallyConnectableInfo> info =
      ExternallyConnectableInfo::FromValue(*externally_connectable,
                                           &install_warnings, error);
  if (!info)
    return false;

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
    std::vector<InstallWarning>* install_warnings,
    std::u16string* error) {
  auto externally_connectable = ExternallyConnectable::FromValue(value);
  if (!externally_connectable.has_value()) {
    *error = std::move(externally_connectable).error();
    return nullptr;
  }

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
        return nullptr;
      }

      matches.AddPattern(pattern);
    }
  }

  std::vector<ExtensionId> ids;
  bool all_ids = false;

  if (externally_connectable->ids) {
    for (const auto& id : *externally_connectable->ids) {
      if (id == kAllIds) {
        all_ids = true;
      } else if (crx_file::id_util::IdIsValid(id)) {
        ids.push_back(id);
      } else {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            externally_connectable_errors::kErrorInvalidId, id);
        return nullptr;
      }
    }
  }

  if (!all_ids && matches.is_empty() && ids.empty()) {
    install_warnings->emplace_back(
        externally_connectable_errors::kErrorNothingSpecified,
        keys::kExternallyConnectable);
  }

  bool accepts_tls_channel_id =
      externally_connectable->accepts_tls_channel_id.value_or(false);

  if (externally_connectable->accepts_tls_channel_id && matches.is_empty()) {
    accepts_tls_channel_id = false;
    install_warnings->emplace_back(
        externally_connectable_errors::kErrorUnusedAcceptsTlsChannelId,
        keys::kExternallyConnectable);
  }

  return base::WrapUnique(new ExternallyConnectableInfo(
      std::move(matches), ids, all_ids, accepts_tls_channel_id));
}

ExternallyConnectableInfo::~ExternallyConnectableInfo() {
}

ExternallyConnectableInfo::ExternallyConnectableInfo(
    URLPatternSet matches,
    const std::vector<ExtensionId>& ids,
    bool all_ids,
    bool accepts_tls_channel_id)
    : matches(std::move(matches)),
      ids(Sorted(ids)),
      all_ids(all_ids),
      accepts_tls_channel_id(accepts_tls_channel_id) {}

bool ExternallyConnectableInfo::IdCanConnect(const ExtensionId& id) {
  if (all_ids)
    return true;
  DCHECK(base::ranges::is_sorted(ids));
  return std::binary_search(ids.begin(), ids.end(), id);
}

}  // namespace extensions
