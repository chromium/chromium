// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_REL32_FINDER_X64_H_
#define COURGETTE_REL32_FINDER_X64_H_

#include <stdint.h>

#include <vector>

#include "courgette/image_utils.h"
#include "courgette/rel32_finder.h"

namespace courgette {

// This implementation performs naive scan for opcodes having rel32 as
// arguments, disregarding instruction alignment.
class Rel32FinderX64 : public Rel32Finder {
 public:
  Rel32FinderX64(RVA relocs_start_rva, RVA relocs_end_rva, RVA size_of_image);
  ~Rel32FinderX64() override = default;

  void Find(const uint8_t* start_pointer,
            const uint8_t* end_pointer,
            RVA start_rva,
            RVA end_rva,
            const std::vector<RVA>& abs32_locations) override;

 private:
  uint32_t size_of_image_;
};

}  // namespace courgette

#endif  // COURGETTE_REL32_FINDER_X64_H_
