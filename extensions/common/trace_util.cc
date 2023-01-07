// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/trace_util.h"

#include "content/public/common/pseudonymization_util.h"

namespace extensions {

void ExtensionIdForTracing::WriteIntoTrace(
    perfetto::TracedProto<perfetto::protos::pbzero::ChromeExtensionId> proto)
    const {
  proto->set_extension_id(extension_id_);
  proto->set_pseudonymized_extension_id(
      content::PseudonymizationUtil::PseudonymizeString(extension_id_));
}

}  // namespace extensions
