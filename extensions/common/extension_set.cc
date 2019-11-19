// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_set.h"

#include "extensions/common/constants.h"
#include "extensions/common/url_pattern_set.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

namespace {

ExtensionId GetExtensionIdByURL(const GURL& url) {
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

}  // namespace

ExtensionSet::const_iterator::const_iterator() {}

ExtensionSet::const_iterator::const_iterator(const const_iterator& other)
    : it_(other.it_) {
}

ExtensionSet::const_iterator::const_iterator(ExtensionMap::const_iterator it)
    : it_(it) {
}

ExtensionSet::const_iterator::~const_iterator() {}

ExtensionSet::ExtensionSet() {
}

ExtensionSet::~ExtensionSet() {
}

size_t ExtensionSet::size() const {
  return extensions_.size();
}

bool ExtensionSet::is_empty() const {
  return extensions_.empty();
}

bool ExtensionSet::Contains(const ExtensionId& extension_id) const {
  return extensions_.find(extension_id) != extensions_.end();
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
  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    Insert(*iter);
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
  // TODO(crbug/852162): Add support for blob: URLs in MatchesURL.
  const Extension* extension = GetHostedAppByURL(url);
  if (!extension)
    return ExtensionId();

  return extension->id();
}

const Extension* ExtensionSet::GetExtensionOrAppByURL(const GURL& url) const {
  ExtensionId extension_id = GetExtensionIdByURL(url);
  if (!extension_id.empty())
    return GetByID(extension_id);

  // GetHostedAppByURL already supports filesystem: URLs (via MatchesURL).
  // TODO(crbug/852162): Add support for blob: URLs in MatchesURL.
  return GetHostedAppByURL(url);
}

const Extension* ExtensionSet::GetAppByURL(const GURL& url) const {
  const Extension* extension = GetExtensionOrAppByURL(url);
  return (extension && extension->is_app()) ? extension : NULL;
}

const Extension* ExtensionSet::GetHostedAppByURL(const GURL& url) const {
  for (auto iter = extensions_.cbegin(); iter != extensions_.cend(); ++iter) {
    if (iter->second->web_extent().MatchesURL(url))
      return iter->second.get();
  }

  return NULL;
}

const Extension* ExtensionSet::GetHostedAppByOverlappingWebExtent(
    const URLPatternSet& extent) const {
  for (auto iter = extensions_.cbegin(); iter != extensions_.cend(); ++iter) {
    if (iter->second->web_extent().OverlapsWith(extent))
      return iter->second.get();
  }

  return NULL;
}

bool ExtensionSet::InSameExtent(const GURL& old_url,
                                const GURL& new_url) const {
  return GetExtensionOrAppByURL(old_url) ==
      GetExtensionOrAppByURL(new_url);
}

const Extension* ExtensionSet::GetByID(const ExtensionId& id) const {
  auto i = extensions_.find(id);
  if (i != extensions_.end())
    return i->second.get();
  return nullptr;
}

ExtensionIdSet ExtensionSet::GetIDs() const {
  ExtensionIdSet ids;
  for (auto it = extensions_.cbegin(); it != extensions_.cend(); ++it) {
    ids.insert(it->first);
  }
  return ids;
}

bool ExtensionSet::ExtensionBindingsAllowed(const GURL& url) const {
  if (url.SchemeIs(kExtensionScheme))
    return true;

  for (auto it = extensions_.cbegin(); it != extensions_.cend(); ++it) {
    if (it->second->location() == Manifest::COMPONENT &&
        it->second->web_extent().MatchesURL(url))
      return true;
  }

  return false;
}

}  // namespace extensions
