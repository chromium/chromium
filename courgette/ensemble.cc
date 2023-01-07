// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/ensemble.h"

#include <stddef.h>
#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "courgette/program_detector.h"
#include "courgette/region.h"
#include "courgette/simple_delta.h"
#include "courgette/streams.h"

namespace courgette {

Element::Element(ExecutableType kind,
                 Ensemble* ensemble,
                 const Region& region)
    : kind_(kind), ensemble_(ensemble), region_(region) {
}

Element::~Element() = default;

std::string Element::Name() const {
  return ensemble_->name() + "(" + base::NumberToString(kind()) + "," +
         base::NumberToString(offset_in_ensemble()) + "," +
         base::NumberToString(region().length()) + ")";
}

// Scans the Ensemble's region, sniffing out Elements.  We assume that the
// elements do not overlap.
Status Ensemble::FindEmbeddedElements() {

  size_t length = region_.length();
  const uint8_t* start = region_.start();

  size_t position = 0;
  while (position < length) {
    ExecutableType type;
    size_t detected_length;
    Status result = DetectExecutableType(start + position,
                                         length - position,
                                         &type, &detected_length);
    if (result == C_OK) {
      Region region(start + position, detected_length);

      Element* element = new Element(type, this, region);
      owned_elements_.push_back(element);
      elements_.push_back(element);
      position += region.length();
    } else {
      position++;
    }
  }
  return C_OK;
}

Ensemble::~Ensemble() {
  for (size_t i = 0;  i < owned_elements_.size();  ++i)
    delete owned_elements_[i];
}

}  // namespace
