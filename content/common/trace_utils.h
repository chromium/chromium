// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_TRACE_UTILS_H_
#define CONTENT_COMMON_TRACE_UTILS_H_

#include <string>

#include "base/trace_event/traced_value.h"

namespace content {

// This can't be a struct since in C++14 static constexpr structure members
// have external linkage. This has been fixed in C++17.
namespace tracing_category {
static constexpr const char kNavigation[] = "navigation";
}

// Class which facilitates annotating with traces all possible return paths
// from a function or a method. Setting the return reason is enforced by a
// CHECK at runtime to ensure that no return branches have been missed.
// Template usage is necessary since the tracing code requires the category to
// be constant at compile time.
//
// Example usage:
//
// void SomeMethod() {
//   TraceReturnReason<tracing_category::kNavigation> trace_return("Method");
//
//   if (condition) {
//     trace_return.set_return_reason("foo");
//     trace_return.traced_value()->SetBoolean("condition", true);
//     return;
//   }
//
//   trace_return.set_return_reason("default return");
//   return;
// }
//
template <const char* category>
class TraceReturnReason {
 public:
  explicit TraceReturnReason(const char* const name)
      : name_(name),
        traced_value_(std::make_unique<base::trace_event::TracedValue>()) {
    TRACE_EVENT_BEGIN0(category, name_);
  }

  ~TraceReturnReason() {
    CHECK(reason_set_);
    TRACE_EVENT_END1(category, name_, "return", std::move(traced_value_));
  }

  void set_return_reason(const std::string& reason) {
    reason_set_ = true;
    traced_value_->SetString("reason", reason);
  }

  // This method exposes the internal TracedValue member so usage of this
  // class allows easy addition of more data to the end of the event.
  base::trace_event::TracedValue* traced_value() { return traced_value_.get(); }

 private:
  const char* const name_;
  bool reason_set_ = false;
  std::unique_ptr<base::trace_event::TracedValue> traced_value_;
};

}  // namespace content

#endif  // CONTENT_COMMON_TRACE_UTILS_H_
