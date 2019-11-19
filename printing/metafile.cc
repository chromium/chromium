// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/metafile.h"

#include <stdint.h>

#include <vector>

#include "base/files/file.h"
#include "base/numerics/safe_conversions.h"

namespace printing {

MetafilePlayer::MetafilePlayer() = default;

MetafilePlayer::~MetafilePlayer() = default;

Metafile::Metafile() = default;

Metafile::~Metafile() = default;

bool Metafile::GetDataAsVector(std::vector<char>* buffer) const {
  buffer->resize(GetDataSize());
  if (buffer->empty())
    return false;
  return GetData(&buffer->front(),
                 base::checked_cast<uint32_t>(buffer->size()));
}

bool Metafile::SaveTo(base::File* file) const {
  if (!file->IsValid())
    return false;

  std::vector<char> buffer;
  if (!GetDataAsVector(&buffer))
    return false;

  if (!file->WriteAtCurrentPosAndCheck(
          base::as_bytes(base::make_span(buffer)))) {
    DLOG(ERROR) << "Failed to save file.";
    return false;
  }
  return true;
}

}  // namespace printing
