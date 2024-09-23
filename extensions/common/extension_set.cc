// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_set.h"

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/url_pattern_set.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

// static
// TODO(solomonkinard): Take GUID-based dynamic URLs in account. Also,
// disambiguate ExtensionHost.
ExtensionId ExtensionSet::GetExtensionIdByURL(const GURL& url) {
  if (url.SchemeIs(kExtensionScheme))
    return url.host();

  // Trying url::Origin is important to properly handle extension schemes inside
  // blob: and filesystem: URLs, which won't match the extension scheme check
  // above.
  url::Origin origin = url::Origin::Create(url);
  if (origin.scheme() == kExtensionScheme)
    return origin.host();

  return ExtensionId();
}

ExtensionSet::const_iterator::const_iterator() = default;

ExtensionSet::const_iterator::const_iterator(const const_iterator& other)
    : it_(other.it_) {
}

ExtensionSet::const_iterator::const_iterator(ExtensionMap::const_iterator it)
    : it_(it) {
}

ExtensionSet::const_iterator::~const_iterator() = default;

ExtensionSet::ExtensionSet() = default;

ExtensionSet::~ExtensionSet() = default;

ExtensionSet::ExtensionSet(ExtensionSet&&) = default;

ExtensionSet& ExtensionSet::operator=(ExtensionSet&&) noexcept = default;

bool ExtensionSet::Contains(const ExtensionId& extension_id) const {
  return base::Contains(extensions_, extension_id);
}

bool ExtensionSet::Insert(const scoped_refptr<const Extension>& extension) {
  auto iter = extensions_.find(extension->id());
  if (iter != extensions_.end()) {
    iter->second = extension;
    return false;  // Had a previous entry.
  }
  extensions_.emplace(extension->id(), extension);
  return true;  // New entry added.
}

bool ExtensionSet::InsertAll(const ExtensionSet& extensions) {
  size_t before = size();
  for (const auto& extension : extensions) {
    Insert(extension);
  }
  return size() != before;
}

bool ExtensionSet::Remove(const ExtensionId& id) {
  return extensions_.erase(id) > 0;
}

void ExtensionSet::Clear() {
  extensions_.clear();
}

ExtensionId ExtensionSet::GetExtensionOrAppIDByURL(const GURL& url) const {
  ExtensionId extension_id = GetExtensionIdByURL(url);
  if (!extension_id.empty())
    return extension_id;

  // GetHostedAppByURL already supports filesystem: URLs (via MatchesURL).
  // TODO(crbug.com/41394231): Add support for blob: URLs in MatchesURL.
  const Extension* extension = GetHostedAppByURL(url);
  if (!extension)
    return ExtensionId();

  return extension->id();
}

const Extension* ExtensionSet::GetExtensionOrAppByURL(const GURL& url,
                                                      bool include_guid) const {
  ExtensionId extension_id = GetExtensionIdByURL(url);
  if (!extension_id.empty())
    return include_guid ? GetByIDorGUID(extension_id) : GetByID(extension_id);

  // GetHostedAppByURL already supports filesystem: URLs (via MatchesURL).
  // TODO(crbug.com/41394231): Add support for blob: URLs in MatchesURL.
  return GetHostedAppByURL(url);
}

const Extension* ExtensionSet::GetAppByURL(const GURL& url) const {
  const Extension* extension = GetExtensionOrAppByURL(url);
  return (extension && extension->is_app()) ? extension : nullptr;
}

const Extension* ExtensionSet::GetHostedAppByURL(const GURL& url) const {
  auto hosted_app_itr =
      base::ranges::find_if(extensions_, [&](const auto& extension_info) {
        return extension_info.second->web_extent().MatchesURL(url);
      });
  return hosted_app_itr != extensions_.end() ? hosted_app_itr->second.get()
                                             : nullptr;
}

const Extension* ExtensionSet::GetHostedAppByOverlappingWebExtent(
    const URLPatternSet& extent) const {
  auto hosted_app_itr =
      base::ranges::find_if(extensions_, [&](const auto& extension_info) {
        return extension_info.second->web_extent().OverlapsWith(extent);
      });
  return hosted_app_itr != extensions_.end() ? hosted_app_itr->second.get()
                                             : nullptr;
}

bool ExtensionSet::InSameExtent(const GURL& old_url,
                                const GURL& new_url) const {
  return GetExtensionOrAppByURL(old_url) ==
      GetExtensionOrAppByURL(new_url);
}

const Extension* ExtensionSet::GetByID(const ExtensionId& id) const {
  return base::FindPtrOrNull(extensions_, id);
}

const Extension* ExtensionSet::GetByGUID(const std::string& guid) const {
  auto extension_itr = base::ranges::find(
      extensions_, guid,
      [](const auto& extension_info) { return extension_info.second->guid(); });
  return extension_itr != extensions_.end() ? extension_itr->second.get()
                                            : nullptr;
}

const Extension* ExtensionSet::GetByIDorGUID(
    const std::string& id_or_guid) const {
  if (auto* extension = GetByID(id_or_guid))
    return extension;
  return GetByGUID(id_or_guid);
}

ExtensionIdSet ExtensionSet::GetIDs() const {
  ExtensionIdSet ids;
  for (const auto& [extension_id, extension] : extensions_) {
    ids.insert(extension_id);
  }
  return ids;
}

bool ExtensionSet::ExtensionBindingsAllowed(const GURL& url) const {
  if (url.SchemeIs(kExtensionScheme))
    return true;

  return base::ranges::any_of(extensions_, [&url](const auto& extension_info) {
    const Extension* extension = extension_info.second.get();
    return extension->location() == mojom::ManifestLocation::kComponent &&
           extension->web_extent().MatchesURL(url);
  });
}

}  // namespace extensions
