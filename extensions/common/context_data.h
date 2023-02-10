// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CONTEXT_DATA_H_
#define EXTENSIONS_COMMON_CONTEXT_DATA_H_

#include <memory>

#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

// An interface that contains data about the current context. Additionally, it
// abstracts away differences in data between the browser and renderer, for
// example between a RenderFrameHost and RenderFrame.
// TODO(b/267673751): Adjust ContextData to hold more data.
class ContextData {
 public:
  virtual ~ContextData() = default;
  virtual std::unique_ptr<ContextData> Clone() const = 0;
  virtual std::unique_ptr<ContextData> GetLocalParentOrOpener() const = 0;
  virtual GURL GetUrl() const = 0;
  virtual url::Origin GetOrigin() const = 0;
  virtual bool CanAccess(const url::Origin& target) const = 0;
  virtual bool CanAccess(const ContextData& target) const = 0;
  virtual uintptr_t GetId() const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FRAME_ADAPTER_H_
