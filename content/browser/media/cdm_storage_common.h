// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_STORAGE_COMMON_H_
#define CONTENT_BROWSER_MEDIA_CDM_STORAGE_COMMON_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "media/cdm/cdm_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

// CdmStorage provides per-storage key, per-CDM type storage.
struct CONTENT_EXPORT CdmStorageBindingContext {
  CdmStorageBindingContext(const blink::StorageKey& storage_key,
                           const media::CdmType& cdm_type)
      : storage_key(storage_key), cdm_type(cdm_type) {}

  const blink::StorageKey storage_key;
  const media::CdmType cdm_type;
};

// A CDM file for a given storage key can be uniquely identified by its name
// and CDM type.
struct CONTENT_EXPORT CdmFileId {
  CdmFileId(const std::string& name, const media::CdmType& cdm_type);
  CdmFileId(const CdmFileId&);
  ~CdmFileId();

  bool operator==(const CdmFileId& rhs) const {
    return (name == rhs.name) && (cdm_type == rhs.cdm_type);
  }
  bool operator<(const CdmFileId& rhs) const {
    return std::tie(name, cdm_type) < std::tie(rhs.name, rhs.cdm_type);
  }

  const std::string name;
  const media::CdmType cdm_type;
};

struct CONTENT_EXPORT CdmFileIdAndContents {
  CdmFileIdAndContents(const CdmFileId& file, std::vector<uint8_t> data);
  CdmFileIdAndContents(const CdmFileIdAndContents&);
  ~CdmFileIdAndContents();

  const CdmFileId file;
  const std::vector<uint8_t> data;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_STORAGE_COMMON_H_
