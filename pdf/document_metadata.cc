// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/document_metadata.h"

namespace chrome_pdf {

DocumentMetadata::DocumentMetadata() = default;

DocumentMetadata::DocumentMetadata(DocumentMetadata&&) noexcept = default;

DocumentMetadata& DocumentMetadata::operator=(
    DocumentMetadata&& other) noexcept = default;

DocumentMetadata::~DocumentMetadata() = default;

}  // namespace chrome_pdf
