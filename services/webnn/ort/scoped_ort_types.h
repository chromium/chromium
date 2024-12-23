// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_SCOPED_ORT_TYPES_H
#define SERVICES_WEBNN_ORT_SCOPED_ORT_TYPES_H

#include <memory>

#include "third_party/microsoft_dxheaders/include/onnxruntime_c_api.h"

namespace webnn::ort {

class ScopedOrtValue {
 public:
  ScopedOrtValue();
  ~ScopedOrtValue();

  OrtValue* get_ptr() { return *pptr_; }
  OrtValue** get_pptr() { return pptr_.get(); }

 private:
  std::unique_ptr<OrtValue*> pptr_;
};

class ScopedOrtMemoryInfo {
 public:
  ScopedOrtMemoryInfo();
  ~ScopedOrtMemoryInfo();

  OrtMemoryInfo* get_ptr() { return *pptr_; }
  OrtMemoryInfo** get_pptr() { return pptr_.get(); }

 private:
  std::unique_ptr<OrtMemoryInfo*> pptr_;
};

class ScopedOrtOpAttr {
 public:
  ScopedOrtOpAttr();
  ~ScopedOrtOpAttr();

  OrtOpAttr* get_ptr() { return *pptr_; }
  OrtOpAttr** get_pptr() { return pptr_.get(); }

 private:
  std::unique_ptr<OrtOpAttr*> pptr_;
};

class ScopedOrtGraph {
 public:
  ScopedOrtGraph();
  ~ScopedOrtGraph();

  OrtGraph* get_ptr() { return *pptr_; }
  OrtGraph** get_pptr() { return pptr_.get(); }

 private:
  std::unique_ptr<OrtGraph*> pptr_;
};

class ScopedOrtShape {
 public:
  ScopedOrtShape();
  ~ScopedOrtShape();

  OrtShape* get_ptr() { return *pptr_; }
  OrtShape** get_pptr() { return pptr_.get(); }

 private:
  std::unique_ptr<OrtShape*> pptr_;
};

class ScopedOrtValueInfo {
 public:
  ScopedOrtValueInfo();
  ~ScopedOrtValueInfo();

  OrtValueInfo* get_ptr() { return *pptr_; }
  OrtValueInfo** get_pptr() { return pptr_.get(); }

 private:
  std::unique_ptr<OrtValueInfo*> pptr_;
};

class ScopedOrtNode {
 public:
  ScopedOrtNode();
  ~ScopedOrtNode();

  OrtNode* get_ptr() { return *pptr_; }
  OrtNode** get_pptr() { return pptr_.get(); }

 private:
  std::unique_ptr<OrtNode*> pptr_;
};

class ScopedOrtModel {
 public:
  ScopedOrtModel();
  ~ScopedOrtModel();

  OrtModel* get_ptr() { return *pptr_; }
  OrtModel** get_pptr() { return pptr_.get(); }

 private:
  std::unique_ptr<OrtModel*> pptr_;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_SCOPED_ORT_TYPES_H
