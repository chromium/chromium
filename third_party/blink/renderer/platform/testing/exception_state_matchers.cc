// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/exception_state_matchers.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

namespace blink {

void PrintTo(const ExceptionState& exception_state, std::ostream* os) {
  if (!exception_state.HadException()) {
    *os << "no exception";
    return;
  }

  *os << internal::ExceptionCodeToString(exception_state.Code()) << ": "
      << exception_state.Message();
}

namespace internal {

std::string ExceptionCodeToString(ExceptionCode code) {
  if (IsDOMExceptionCode(code)) {
    return DOMException::GetErrorName(static_cast<DOMExceptionCode>(code))
               .Ascii() +
           " DOMException";
  }

  return base::StrCat({"exception with code ", base::NumberToString(code)});
}

}  // namespace internal

}  // namespace blink
