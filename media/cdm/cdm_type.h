// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_TYPE_H_
#define MEDIA_CDM_CDM_TYPE_H_

#include "base/token.h"

#include "media/base/media_export.h"  // nogncheck

namespace media {

// TODO(crbug.com/1231162): Remove the `legacy_file_system_id` field and make
// this a base::Token alias once CDM data has been migrated off of the
// PluginPrivateFileSystem. Then we can remove this file and resolve the
// dependency issues associated with this file ("nogncheck" comments in
// third_party/widevine/cdm/widevine_cdm_common.h and media/cdm/cdm_type.h)
struct MEDIA_EXPORT CdmType {
  // A token to uniquely identify the type of the CDM. Used for per-CDM-type
  // isolation, e.g. for running different CDMs in different child processes,
  // and per-CDM-type storage. A zero token indicates that this CdmType should
  // not have a corresponding CdmStorage.
  base::Token id;

  // Identifier used by the PluginPrivateFileSystem to identify the files stored
  // by this CDM. Valid identifiers only contain letters (A-Za-z), digits(0-9),
  // or "._-".
  const char* legacy_file_system_id = "";

  bool operator==(const CdmType& other) const {
    return (this->id == other.id) && (strcmp(this->legacy_file_system_id,
                                             other.legacy_file_system_id) == 0);
  }

  bool operator<(const CdmType& other) const {
    return (this->id < other.id) || (strcmp(this->legacy_file_system_id,
                                            other.legacy_file_system_id) < 0);
  }
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_TYPE_H_
