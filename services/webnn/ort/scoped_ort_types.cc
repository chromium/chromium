// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/scoped_ort_types.h"

#include <memory>

#include "services/webnn/ort/utils_ort.h"

namespace webnn::ort {

ScopedOrtValue::ScopedOrtValue() {
  pptr_ = std::make_unique<OrtValue*>(nullptr);
}
ScopedOrtValue::~ScopedOrtValue() {
  // TODO: use deleter instead.
  GetOrtApi()->ReleaseValue(*pptr_);
}

ScopedOrtMemoryInfo::ScopedOrtMemoryInfo() {
  pptr_ = std::make_unique<OrtMemoryInfo*>(nullptr);
}
ScopedOrtMemoryInfo::~ScopedOrtMemoryInfo() {
  GetOrtApi()->ReleaseMemoryInfo(*pptr_);
}

ScopedOrtOpAttr::ScopedOrtOpAttr() {
  pptr_ = std::make_unique<OrtOpAttr*>(nullptr);
}
ScopedOrtOpAttr::~ScopedOrtOpAttr() {
  GetOrtApi()->ReleaseOpAttr(*pptr_);
}

ScopedOrtGraph::ScopedOrtGraph() {
  pptr_ = std::make_unique<OrtGraph*>(nullptr);
}
ScopedOrtGraph::~ScopedOrtGraph() {
  GetOrtGraphApi()->ReleaseGraph(*pptr_);
}

ScopedOrtShape::ScopedOrtShape() {
  pptr_ = std::make_unique<OrtShape*>(nullptr);
}
ScopedOrtShape::~ScopedOrtShape() {
  GetOrtGraphApi()->ReleaseShape(*pptr_);
}

ScopedOrtValueInfo::ScopedOrtValueInfo() {
  pptr_ = std::make_unique<OrtValueInfo*>(nullptr);
}
ScopedOrtValueInfo::~ScopedOrtValueInfo() {
  GetOrtGraphApi()->ReleaseValueInfo(*pptr_);
}

ScopedOrtNode::ScopedOrtNode() {
  pptr_ = std::make_unique<OrtNode*>(nullptr);
}
ScopedOrtNode::~ScopedOrtNode() {
  GetOrtGraphApi()->ReleaseNode(*pptr_);
}

ScopedOrtModel::ScopedOrtModel() {
  pptr_ = std::make_unique<OrtModel*>(nullptr);
}
ScopedOrtModel::~ScopedOrtModel() {
  GetOrtGraphApi()->ReleaseModel(*pptr_);
}

}  // namespace webnn::ort
