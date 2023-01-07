// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/rel32_finder.h"

namespace courgette {

Rel32Finder::Rel32Finder(RVA relocs_start_rva, RVA relocs_end_rva)
    : relocs_start_rva_(relocs_start_rva), relocs_end_rva_(relocs_end_rva) {}

void Rel32Finder::SwapRel32Locations(std::vector<RVA>* dest) {
  dest->swap(rel32_locations_);
}

#if COURGETTE_HISTOGRAM_TARGETS
void Rel32FinderX86::SwapRel32TargetRVAs(std::map<RVA, int>* dest) {
  dest->swap(rel32_target_rvas_);
}
#endif

}  // namespace courgette