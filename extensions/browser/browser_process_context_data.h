// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BROWSER_PROCESS_CONTEXT_DATA_H_
#define EXTENSIONS_BROWSER_BROWSER_PROCESS_CONTEXT_DATA_H_

#include <memory>

#include "extensions/common/process_context_data.h"
#include "url/origin.h"

namespace content {
class RenderProcessHost;
}

namespace extensions {

class BrowserProcessContextData : public ProcessContextData {
 public:
  explicit BrowserProcessContextData(content::RenderProcessHost* process)
      : process_(process) {
    CHECK(process_);
  }

  ~BrowserProcessContextData() override = default;

  std::unique_ptr<ProcessContextData> CloneProcessContextData() const override;
  bool HasControlledFrameCapability() const override;

 private:
  const raw_ptr<content::RenderProcessHost> process_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BROWSER_PROCESS_CONTEXT_DATA_H_
