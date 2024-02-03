// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/annotation_selector.h"

#include <optional>

#include "third_party/blink/renderer/core/annotation/text_annotation_selector.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {
std::optional<AnnotationSelector::GeneratorFunc>& GetGeneratorForTesting() {
  DEFINE_STATIC_LOCAL(std::optional<AnnotationSelector::GeneratorFunc>,
                      generator, ());
  return generator;
}
}  //  namespace

// static
void AnnotationSelector::SetGeneratorForTesting(GeneratorFunc generator) {
  GetGeneratorForTesting() = generator;
}

// static
void AnnotationSelector::UnsetGeneratorForTesting() {
  GetGeneratorForTesting().reset();
}

// static
AnnotationSelector* AnnotationSelector::Deserialize(const String& serialized) {
  if (GetGeneratorForTesting()) {
    return GetGeneratorForTesting()->Run(serialized);
  }

  // TODO(bokan): This should check the `serialized` string for a type and then
  // delegate out to a Deserialize function in the correct class. The current
  // implementation, using the text directive syntax, is temporary until we
  // determine a serialization format.
  return MakeGarbageCollected<TextAnnotationSelector>(
      TextFragmentSelector::FromTextDirective(serialized));
}

}  // namespace blink
