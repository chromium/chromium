// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_STYLE_PROPERTY_MAP_READ_ONLY_MAIN_THREAD_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_STYLE_PROPERTY_MAP_READ_ONLY_MAIN_THREAD_H_

#include "third_party/blink/renderer/core/css/cssom/style_property_map_read_only.h"

namespace blink {

class CSSProperty;
class CSSPropertyName;

class CORE_EXPORT StylePropertyMapReadOnlyMainThread
    : public StylePropertyMapReadOnly {
 public:
  using StylePropertyMapEntry = std::pair<String, CSSStyleValueVector>;

  ~StylePropertyMapReadOnlyMainThread() override = default;

  CSSStyleValue* get(const ExecutionContext*,
                     const String& property_name,
                     ExceptionState&) const override;
  CSSStyleValueVector getAll(const ExecutionContext*,
                             const String& property_name,
                             ExceptionState&) const override;
  bool has(const ExecutionContext*,
           const String& property_name,
           ExceptionState&) const override;

  unsigned int size() const override = 0;

 protected:
  StylePropertyMapReadOnlyMainThread() = default;

  virtual const CSSValue* GetProperty(CSSPropertyID) const = 0;
  virtual const CSSValue* GetCustomProperty(AtomicString) const = 0;

  using IterationCallback =
      std::function<void(const CSSPropertyName&, const CSSValue&)>;
  virtual void ForEachProperty(const IterationCallback&) = 0;

  virtual String SerializationForShorthand(const CSSProperty&) const = 0;

 private:
  IterationSource* StartIteration(ScriptState*, ExceptionState&) override;

  CSSStyleValue* GetShorthandProperty(const CSSProperty&) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(StylePropertyMapReadOnlyMainThread);
};

}  // namespace blink

#endif
