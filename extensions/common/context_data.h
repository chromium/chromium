// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CONTEXT_DATA_H_
#define EXTENSIONS_COMMON_CONTEXT_DATA_H_

#include <memory>

#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

// ContextData is an interface that supports a simple API to verify whether
// a given context is an isolated application. It is used as a base class for
// FrameContextData, which is an abstract class that's used by concrete classes
// to implement both the ContextData and FrameContextData APIs. This class
// allows browser- and renderer-based code to implement these APIs based off
// of different underlying types, like RenderFrameHost vs RenderFrame.
// TODO(b/267673751): Adjust ContextData to hold more data.
class ContextData {
 public:
  virtual ~ContextData() = default;
  virtual std::unique_ptr<ContextData> Clone() const = 0;
  virtual bool IsIsolatedApplication() const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FRAME_ADAPTER_H_
