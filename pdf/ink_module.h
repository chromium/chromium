// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_MODULE_H_
#define PDF_INK_MODULE_H_

#include "base/values.h"
#include "pdf/buildflags.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace chrome_pdf {

class InkModule {
 public:
  InkModule();
  InkModule(const InkModule&) = delete;
  InkModule& operator=(const InkModule&) = delete;
  ~InkModule();

  bool enabled() const { return enabled_; }

  // Returns whether the message was handled or not.
  bool OnMessage(const base::Value::Dict& message);

 private:
  void HandleSetAnnotationModeMessage(const base::Value::Dict& message);

  bool enabled_ = false;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_MODULE_H_
