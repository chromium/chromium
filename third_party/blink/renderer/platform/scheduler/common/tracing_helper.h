// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_TRACING_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_TRACING_HELPER_H_

#include <string>
#include <unordered_set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace scheduler {

// DISCLAIMER
// Using these constants in TRACE_EVENTs is discouraged nor should you pass any
// non-literal string as a category, unless familiar with tracing internals.
// The constants are implemented as static members of a class to have an unique
// address and not violate ODR.
struct PLATFORM_EXPORT TracingCategoryName {
  static constexpr const char kTopLevel[] = "toplevel";
  static constexpr const char kDefault[] = "renderer.scheduler";
  static constexpr const char kInfo[] =
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler");
  static constexpr const char kDebug[] =
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.debug");
};

namespace internal {

PLATFORM_EXPORT void ValidateTracingCategory(const char* category);

}  // namespace internal

PLATFORM_EXPORT std::string PointerToString(const void* pointer);

PLATFORM_EXPORT double TimeDeltaToMilliseconds(const base::TimeDelta& value);

PLATFORM_EXPORT const char* YesNoStateToString(bool is_yes);

class TraceableVariable;

// Unfortunately, using |base::trace_event::TraceLog::EnabledStateObserver|
// wouldn't be helpful in our case because removing one takes linear time
// and tracers may be created and disposed frequently.
class PLATFORM_EXPORT TraceableVariableController {
  DISALLOW_NEW();

 public:
  TraceableVariableController();
  ~TraceableVariableController();

  // Not thread safe.
  void RegisterTraceableVariable(TraceableVariable* traceable_variable);
  void DeregisterTraceableVariable(TraceableVariable* traceable_variable);

  void OnTraceLogEnabled();

 private:
  std::unordered_set<TraceableVariable*> traceable_variables_;
};

class TraceableVariable {
 public:
  TraceableVariable(TraceableVariableController* controller)
      : controller_(controller) {
    controller_->RegisterTraceableVariable(this);
  }

  virtual ~TraceableVariable() {
    controller_->DeregisterTraceableVariable(this);
  }

  virtual void OnTraceLogEnabled() = 0;

 private:
  TraceableVariableController* const controller_;  // Not owned.
};

// TRACE_EVENT macros define static variable to cache a pointer to the state
// of category. Hence, we need distinct version for each category in order to
// prevent unintended leak of state.

template <const char* category>
class StateTracer {
  DISALLOW_NEW();

 public:
  StateTracer(const char* name, const void* object)
      : name_(name), object_(object), slice_is_open_(false) {
    internal::ValidateTracingCategory(category);
  }

  ~StateTracer() {
    if (slice_is_open_)
      TRACE_EVENT_ASYNC_END0(category, name_, object_);
  }

  // String will be copied before leaving this function.
  void TraceString(const String& state) {
    TraceImpl(state.Utf8().c_str(), true);
  }

  // Trace compile-time defined const string, so no copy needed.
  // Null may be passed to indicate the absence of state.
  void TraceCompileTimeString(const char* state) { TraceImpl(state, false); }

 protected:
  bool is_enabled() const {
    bool result = false;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(category, &result);  // Cached.
    return result;
  }

 private:
  void TraceImpl(const char* state, bool need_copy) {
    if (slice_is_open_) {
      TRACE_EVENT_ASYNC_END0(category, name_, object_);
      slice_is_open_ = false;
    }
    if (!state || !is_enabled())
      return;

    // Trace viewer logic relies on subslice starting at the exact same time
    // as the async event.
    base::TimeTicks now = TRACE_TIME_TICKS_NOW();
    TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP0(category, name_, object_, now);
    if (need_copy) {
      TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(category, name_, object_,
                                                  TRACE_STR_COPY(state), now);
    } else {
      TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(category, name_, object_,
                                                  state, now);
    }
    slice_is_open_ = true;
  }

  const char* const name_;    // Not owned.
  const void* const object_;  // Not owned.

  // We have to track whether slice is open to avoid confusion since assignment,
  // "absent" state and OnTraceLogEnabled can happen anytime.
  bool slice_is_open_;

  DISALLOW_COPY_AND_ASSIGN(StateTracer);
};

// TODO(kraynov): Rename to something less generic and reflecting
// the enum nature of such variables.
template <typename T, const char* category>
class TraceableState : public TraceableVariable, private StateTracer<category> {
 public:
  // Converter must return compile-time defined const strings because tracing
  // will not make a copy of them.
  using ConverterFuncPtr = const char* (*)(T);

  TraceableState(T initial_state,
                 const char* name,
                 const void* object,
                 TraceableVariableController* controller,
                 ConverterFuncPtr converter)
      : TraceableVariable(controller),
        StateTracer<category>(name, object),
        converter_(converter),
        state_(initial_state) {
    Trace();
  }

  ~TraceableState() override = default;

  TraceableState& operator=(const T& value) {
    Assign(value);
    return *this;
  }
  TraceableState& operator=(const TraceableState& another) {
    Assign(another.state_);
    return *this;
  }

  operator T() const { return state_; }
  const T& get() const { return state_; }

  void OnTraceLogEnabled() final { Trace(); }

 protected:
  void Assign(T new_state) {
    if (state_ != new_state) {
      state_ = new_state;
      Trace();
    }
  }

  void (*mock_trace_for_test_)(const char*) = nullptr;

 private:
  void Trace() {
    if (UNLIKELY(mock_trace_for_test_)) {
      mock_trace_for_test_(converter_(state_));
      return;
    }

    // Null state string means the absence of state.
    const char* state_str = nullptr;
    if (StateTracer<category>::is_enabled()) {
      state_str = converter_(state_);
    }

    // We have to be explicit to deal with two-phase name lookup in templates:
    // http://blog.llvm.org/2009/12/dreaded-two-phase-name-lookup.html
    StateTracer<category>::TraceCompileTimeString(state_str);
  }

  const ConverterFuncPtr converter_;
  T state_;

  DISALLOW_COPY(TraceableState);
};

template <typename T, const char* category>
class TraceableCounter : public TraceableVariable {
 public:
  using ConverterFuncPtr = double (*)(const T&);

  TraceableCounter(T initial_value,
                   const char* name,
                   const void* object,
                   TraceableVariableController* controller,
                   ConverterFuncPtr converter)
      : TraceableVariable(controller),
        name_(name),
        object_(object),
        converter_(converter),
        value_(initial_value) {
    internal::ValidateTracingCategory(category);
    Trace();
  }

  TraceableCounter(T initial_value,
                   const char* name,
                   const void* object,
                   TraceableVariableController* controller)
      : TraceableVariable(controller),
        name_(name),
        object_(object),
        converter_([](const T& value) { return static_cast<double>(value); }),
        value_(initial_value) {
    internal::ValidateTracingCategory(category);
    Trace();
  }

  TraceableCounter& operator=(const T& value) {
    value_ = value;
    Trace();
    return *this;
  }
  TraceableCounter& operator=(const TraceableCounter& another) {
    value_ = another.value_;
    Trace();
    return *this;
  }

  TraceableCounter& operator+=(const T& value) {
    value_ += value;
    Trace();
    return *this;
  }
  TraceableCounter& operator-=(const T& value) {
    value_ -= value;
    Trace();
    return *this;
  }

  const T& value() const { return value_; }
  const T* operator->() const { return &value_; }
  operator T() const { return value_; }

  void OnTraceLogEnabled() final { Trace(); }

  void Trace() const {
    TRACE_COUNTER_ID1(category, name_, object_, converter_(value_));
  }

 private:
  const char* const name_;    // Not owned.
  const void* const object_;  // Not owned.
  const ConverterFuncPtr converter_;

  T value_;
  DISALLOW_COPY(TraceableCounter);
};

// Add operators when it's needed.

template <typename T, const char* category>
constexpr T operator-(const TraceableCounter<T, category>& counter) {
  return -counter.value();
}

template <typename T, const char* category>
constexpr T operator/(const TraceableCounter<T, category>& lhs, const T& rhs) {
  return lhs.value() / rhs;
}

template <typename T, const char* category>
constexpr bool operator>(const TraceableCounter<T, category>& lhs,
                         const T& rhs) {
  return lhs.value() > rhs;
}

template <typename T, const char* category>
constexpr bool operator<(const TraceableCounter<T, category>& lhs,
                         const T& rhs) {
  return lhs.value() < rhs;
}

template <typename T, const char* category>
constexpr bool operator!=(const TraceableCounter<T, category>& lhs,
                          const T& rhs) {
  return lhs.value() != rhs;
}

template <typename T, const char* category>
constexpr T operator++(TraceableCounter<T, category>& counter) {
  counter = counter.value() + 1;
  return counter.value();
}

template <typename T, const char* category>
constexpr T operator--(TraceableCounter<T, category>& counter) {
  counter = counter.value() - 1;
  return counter.value();
}

template <typename T, const char* category>
constexpr T operator++(TraceableCounter<T, category>& counter, int) {
  T value = counter.value();
  counter = value + 1;
  return value;
}

template <typename T, const char* category>
constexpr T operator--(TraceableCounter<T, category>& counter, int) {
  T value = counter.value();
  counter = value - 1;
  return value;
}

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_TRACING_HELPER_H_
