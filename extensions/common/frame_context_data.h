// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FRAME_CONTEXT_DATA_H_
#define EXTENSIONS_COMMON_FRAME_CONTEXT_DATA_H_

#include <memory>

#include "extensions/common/context_data.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

// FrameContextData is a virtual interface that derives from ContextData, adding
// methods that are frame-specific, like GetUrl(), GetOrigin(), etc. It is
// meant to be a base class for browser- and renderer-based derived classes.
// This class allows abstracting away differences in how these methods are
// implemented between the browser and renderer, for example between a
// RenderFrameHost and a RenderFrame.
// TODO(b/267673751): Adjust ContextData to hold more data.
class FrameContextData : public ContextData {
 public:
  ~FrameContextData() override = default;
  virtual std::unique_ptr<FrameContextData> CloneFrameContextData() const = 0;
  virtual std::unique_ptr<FrameContextData> GetLocalParentOrOpener() const = 0;
  virtual GURL GetUrl() const = 0;
  virtual url::Origin GetOrigin() const = 0;
  virtual bool CanAccess(const url::Origin& target) const = 0;
  virtual bool CanAccess(const FrameContextData& target) const = 0;
  virtual uintptr_t GetId() const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FRAME_CONTEXT_DATA_H_
