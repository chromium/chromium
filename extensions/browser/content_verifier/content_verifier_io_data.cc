// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/content_verifier_io_data.h"

#include <utility>

#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension_id.h"

namespace extensions {

ContentVerifierIOData::ExtensionData::ExtensionData() = default;

ContentVerifierIOData::ExtensionData::ExtensionData(ExtensionData&& other) =
    default;

ContentVerifierIOData::ExtensionData&
ContentVerifierIOData::ExtensionData::operator=(ExtensionData&&) = default;
ContentVerifierIOData::ExtensionData::~ExtensionData() = default;
ContentVerifierIOData::ContentVerifierIOData() = default;
ContentVerifierIOData::~ContentVerifierIOData() = default;

void ContentVerifierIOData::AddData(const ExtensionId& extension_id,
                                    ExtensionData data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  data_map_[extension_id] = std::move(data);
}

void ContentVerifierIOData::RemoveData(const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  data_map_.erase(extension_id);
}

void ContentVerifierIOData::Clear() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  data_map_.clear();
}

const ContentVerifierIOData::ExtensionData* ContentVerifierIOData::GetData(
    const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto found = data_map_.find(extension_id);
  if (found != data_map_.end()) {
    return &found->second;
  }
  return nullptr;
}

}  // namespace extensions
