// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_FACTORY_FOR_TEST_H_
#define PRINTING_PRINTING_CONTEXT_FACTORY_FOR_TEST_H_

#include <memory>

#include "printing/printing_context.h"

namespace printing {

class PrintingContextFactoryForTest {
 public:
  virtual std::unique_ptr<PrintingContext> CreatePrintingContext(
      PrintingContext::Delegate* delegate,
      PrintingContext::ProcessBehavior process_behavior) = 0;
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_FACTORY_FOR_TEST_H_
