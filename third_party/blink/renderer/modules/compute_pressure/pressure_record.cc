// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/pressure_record.h"

namespace blink {

PressureRecord::PressureRecord(V8PressureSource::Enum source,
                               V8PressureState::Enum state,
                               const Vector<V8PressureFactor> factors,
                               const DOMHighResTimeStamp time)
    : source_(source), state_(state), factors_(factors), time_(time) {}

PressureRecord::~PressureRecord() = default;

V8PressureSource PressureRecord::source() const {
  return V8PressureSource(source_);
}

V8PressureState PressureRecord::state() const {
  return V8PressureState(state_);
}

const Vector<V8PressureFactor>& PressureRecord::factors() const {
  return factors_;
}

DOMHighResTimeStamp PressureRecord::time() const {
  return time_;
}

}  // namespace blink
