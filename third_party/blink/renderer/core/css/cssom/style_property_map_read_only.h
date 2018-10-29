// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_STYLE_PROPERTY_MAP_READ_ONLY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_STYLE_PROPERTY_MAP_READ_ONLY_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class CSSProperty;

class CORE_EXPORT StylePropertyMapReadOnly
    : public ScriptWrappable,
      public PairIterable<String, CSSStyleValueVector> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using StylePropertyMapEntry = std::pair<String, CSSStyleValueVector>;

  ~StylePropertyMapReadOnly() override = default;

  CSSStyleValue* get(const ExecutionContext*,
                     const String& property_name,
                     ExceptionState&);
  CSSStyleValueVector getAll(const ExecutionContext*,
                             const String& property_name,
                             ExceptionState&);
  bool has(const ExecutionContext*,
           const String& property_name,
           ExceptionState&);

  virtual unsigned int size() = 0;

 protected:
  StylePropertyMapReadOnly() = default;

  virtual const CSSValue* GetProperty(CSSPropertyID) = 0;
  virtual const CSSValue* GetCustomProperty(AtomicString) = 0;

  using IterationCallback =
      std::function<void(const AtomicString&, const CSSValue&)>;
  virtual void ForEachProperty(const IterationCallback&) = 0;

  virtual String SerializationForShorthand(const CSSProperty&) = 0;

  const CSSValue* GetCustomProperty(const ExecutionContext&,
                                    const AtomicString&);

 private:
  IterationSource* StartIteration(ScriptState*, ExceptionState&) override;

  CSSStyleValue* GetShorthandProperty(const CSSProperty&);

 private:
  DISALLOW_COPY_AND_ASSIGN(StylePropertyMapReadOnly);
};

}  // namespace blink

#endif
