/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_FACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_FACE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/font_display.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptors.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSFontFace;
class CSSValue;
class DOMArrayBuffer;
class DOMArrayBufferView;
class Document;
class ExceptionState;
class FontFaceDescriptors;
class StringOrArrayBufferOrArrayBufferView;
class CSSPropertyValueSet;
class StyleRuleFontFace;

class CORE_EXPORT FontFace : public ScriptWrappable,
                             public ActiveScriptWrappable<FontFace>,
                             public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(FontFace);

 public:
  enum LoadStatusType { kUnloaded, kLoading, kLoaded, kError };

  static FontFace* Create(ExecutionContext*,
                          const AtomicString& family,
                          StringOrArrayBufferOrArrayBufferView&,
                          const FontFaceDescriptors*);
  static FontFace* Create(Document*, const StyleRuleFontFace*);

  explicit FontFace(ExecutionContext*);
  FontFace(ExecutionContext*,
           const AtomicString& family,
           const FontFaceDescriptors*);
  ~FontFace() override;

  const AtomicString& family() const { return family_; }
  String style() const;
  String weight() const;
  String stretch() const;
  String unicodeRange() const;
  String variant() const;
  String featureSettings() const;
  String display() const;

  // FIXME: Changing these attributes should affect font matching.
  void setFamily(ExecutionContext*, const AtomicString& s, ExceptionState&) {
    family_ = s;
  }
  void setStyle(ExecutionContext*, const String&, ExceptionState&);
  void setWeight(ExecutionContext*, const String&, ExceptionState&);
  void setStretch(ExecutionContext*, const String&, ExceptionState&);
  void setUnicodeRange(ExecutionContext*, const String&, ExceptionState&);
  void setVariant(ExecutionContext*, const String&, ExceptionState&);
  void setFeatureSettings(ExecutionContext*, const String&, ExceptionState&);
  void setDisplay(ExecutionContext*, const String&, ExceptionState&);

  String status() const;
  ScriptPromise loaded(ScriptState* script_state) {
    return FontStatusPromise(script_state);
  }

  ScriptPromise load(ScriptState*);

  LoadStatusType LoadStatus() const { return status_; }
  void SetLoadStatus(LoadStatusType);
  void SetError(DOMException* = nullptr);
  DOMException* GetError() const { return error_; }
  FontSelectionCapabilities GetFontSelectionCapabilities() const;
  CSSFontFace* CssFontFace() { return css_font_face_.Get(); }
  size_t ApproximateBlankCharacterCount() const;
  FontDisplay GetFontDisplay() const;

  void Trace(blink::Visitor*) override;

  bool HadBlankText() const;

  class CORE_EXPORT LoadFontCallback : public GarbageCollectedMixin {
   public:
    virtual ~LoadFontCallback() = default;
    virtual void NotifyLoaded(FontFace*) = 0;
    virtual void NotifyError(FontFace*) = 0;
    void Trace(blink::Visitor* visitor) override {}
  };
  void LoadWithCallback(LoadFontCallback*);
  void AddCallback(LoadFontCallback*);

  // ScriptWrappable:
  bool HasPendingActivity() const final;

 private:
  static FontFace* Create(ExecutionContext*,
                          const AtomicString& family,
                          DOMArrayBuffer* source,
                          const FontFaceDescriptors*);
  static FontFace* Create(ExecutionContext*,
                          const AtomicString& family,
                          DOMArrayBufferView*,
                          const FontFaceDescriptors*);
  static FontFace* Create(ExecutionContext*,
                          const AtomicString& family,
                          const String& source,
                          const FontFaceDescriptors*);

  void InitCSSFontFace(ExecutionContext*, const CSSValue& src);
  void InitCSSFontFace(const unsigned char* data, size_t);
  void SetPropertyFromString(const ExecutionContext*,
                             const String&,
                             AtRuleDescriptorID,
                             ExceptionState* = nullptr);
  bool SetPropertyFromStyle(const CSSPropertyValueSet&, AtRuleDescriptorID);
  bool SetPropertyValue(const CSSValue*, AtRuleDescriptorID);
  bool SetFamilyValue(const CSSValue&);
  ScriptPromise FontStatusPromise(ScriptState*);
  void RunCallbacks();

  using LoadedProperty = ScriptPromiseProperty<Member<FontFace>,
                                               Member<FontFace>,
                                               Member<DOMException>>;

  AtomicString family_;
  String ots_parse_message_;
  Member<const CSSValue> style_;
  Member<const CSSValue> weight_;
  Member<const CSSValue> stretch_;
  Member<const CSSValue> unicode_range_;
  Member<const CSSValue> variant_;
  Member<const CSSValue> feature_settings_;
  Member<const CSSValue> display_;
  LoadStatusType status_;
  Member<DOMException> error_;

  Member<LoadedProperty> loaded_property_;
  Member<CSSFontFace> css_font_face_;
  HeapVector<Member<LoadFontCallback>> callbacks_;
  DISALLOW_COPY_AND_ASSIGN(FontFace);
};

using FontFaceArray = HeapVector<Member<FontFace>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_FACE_H_
