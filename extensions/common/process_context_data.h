// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PROCESS_CONTEXT_DATA_H_
#define EXTENSIONS_COMMON_PROCESS_CONTEXT_DATA_H_

#include <memory>

#include "extensions/common/context_data.h"
#include "url/origin.h"

namespace extensions {

// ProcessContextData is a virtual interface that derives from ContextData.
// This class adds an API that allows browser- and renderer-based derived
// classes to be used by common code.
// TODO(b/267673751): Adjust ContextData to hold more data.
class ProcessContextData : public ContextData {
 public:
  ~ProcessContextData() override = default;
  virtual std::unique_ptr<ProcessContextData> CloneProcessContextData()
      const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PROCESS_CONTEXT_DATA_H_
