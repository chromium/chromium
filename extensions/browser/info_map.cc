// Copyright 2013 The Chromium Authors
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

void InfoMap::RemoveExtension(const std::string& extension_id) {
  CheckOnValidThread();
  DCHECK(extensions_.GetByID(extension_id));
  extensions_.Remove(extension_id);
}

void InfoMap::SetContentVerifier(ContentVerifier* verifier) {
  content_verifier_ = verifier;
}

InfoMap::~InfoMap() = default;

}  // namespace extensions
