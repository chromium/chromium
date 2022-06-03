// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/info_map.h"

#include "base/strings/string_util.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

using content::BrowserThread;

namespace extensions {

namespace {

void CheckOnValidThread() { DCHECK_CURRENTLY_ON(BrowserThread::IO); }

}  // namespace

InfoMap::InfoMap() {}

const ExtensionSet& InfoMap::extensions() const {
  CheckOnValidThread();
  return extensions_;
}

void InfoMap::AddExtension(const Extension* extension,
                           base::Time install_time,
                           bool incognito_enabled,
                           bool notifications_disabled) {
  CheckOnValidThread();
  extensions_.Insert(extension);
}

void InfoMap::RemoveExtension(const std::string& extension_id,
                              const UnloadedExtensionReason reason) {
  CheckOnValidThread();
  const Extension* extension = extensions_.GetByID(extension_id);
  bool was_uninstalled = (reason != UnloadedExtensionReason::DISABLE &&
                          reason != UnloadedExtensionReason::TERMINATE);
  if (extension) {
    extensions_.Remove(extension_id);
  } else if (!was_uninstalled) {
    // NOTE: This can currently happen if we receive multiple unload
    // notifications, e.g. setting incognito-enabled state for a
    // disabled extension (e.g., via sync).  See
    // http://code.google.com/p/chromium/issues/detail?id=50582 .
    NOTREACHED() << extension_id;
  }
}

void InfoMap::SetContentVerifier(ContentVerifier* verifier) {
  content_verifier_ = verifier;
}

InfoMap::~InfoMap() = default;

}  // namespace extensions
