// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "courgette/courgette.h"
#include "courgette/courgette_flow.h"
#include "courgette/region.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  courgette::CourgetteFlow flow;
  courgette::RegionBuffer buffer(courgette::Region(data, size));
  flow.ReadDisassemblerFromBuffer(flow.ONLY, buffer);
  flow.CreateAssemblyProgramFromDisassembler(flow.ONLY, false);
  flow.CreateEncodedProgramFromDisassemblerAndAssemblyProgram(flow.ONLY);
  flow.WriteSinkStreamSetFromEncodedProgram(flow.ONLY);
  // Not bothering to check |flow.failed()|.
  return 0;
}
