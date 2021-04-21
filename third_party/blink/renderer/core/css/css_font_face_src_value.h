/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FACE_SRC_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FACE_SRC_VALUE_H_

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/css/css_origin_clean.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;

class CORE_EXPORT CSSFontFaceSrcValue : public CSSValue {
 public:
  static CSSFontFaceSrcValue* Create(const String& specified_resource,
                                     const String& absolute_resource,
                                     const Referrer& referrer,
                                     scoped_refptr<const DOMWrapperWorld> world,
                                     OriginClean origin_clean,
                                     bool is_ad_related) {
    return MakeGarbageCollected<CSSFontFaceSrcValue>(
        specified_resource, absolute_resource, referrer, false,
        std::move(world), origin_clean, is_ad_related);
  }
  static CSSFontFaceSrcValue* CreateLocal(
      const String& absolute_resource,
      scoped_refptr<const DOMWrapperWorld> world,
      OriginClean origin_clean,
      bool is_ad_related) {
    return MakeGarbageCollected<CSSFontFaceSrcValue>(
        g_empty_string, absolute_resource, Referrer(), true, std::move(world),
        origin_clean, is_ad_related);
  }

  CSSFontFaceSrcValue(const String& specified_resource,
                      const String& absolute_resource,
                      const Referrer& referrer,
                      bool local,
                      scoped_refptr<const DOMWrapperWorld> world,
                      OriginClean origin_clean,
                      bool is_ad_related)
      : CSSValue(kFontFaceSrcClass),
        absolute_resource_(absolute_resource),
        specified_resource_(specified_resource),
        referrer_(referrer),
        is_local_(local),
        world_(std::move(world)),
        origin_clean_(origin_clean),
        is_ad_related_(is_ad_related) {}

  const String& GetResource() const { return absolute_resource_; }
  const String& Format() const { return format_; }
  bool IsLocal() const { return is_local_; }

  void SetFormat(const String& format) { format_ = format; }

  bool IsSupportedFormat() const;

  String CustomCSSText() const;

  bool HasFailedOrCanceledSubresources() const;

  FontResource& Fetch(ExecutionContext*, FontResourceClient*) const;

  bool Equals(const CSSFontFaceSrcValue&) const;

  void TraceAfterDispatch(blink::Visitor* visitor) const {
    visitor->Trace(fetched_);
    CSSValue::TraceAfterDispatch(visitor);
  }

 private:
  void RestoreCachedResourceIfNeeded(ExecutionContext*) const;

  const String absolute_resource_;
  const String specified_resource_;
  String format_;
  const Referrer referrer_;
  const bool is_local_;
  const scoped_refptr<const DOMWrapperWorld> world_;
  const OriginClean origin_clean_;
  bool is_ad_related_;

  class FontResourceHelper : public GarbageCollected<FontResourceHelper>,
                             public FontResourceClient {
   public:
    FontResourceHelper(FontResource* resource,
                       base::SingleThreadTaskRunner* task_runner) {
      SetResource(resource, task_runner);
    }

    void Trace(Visitor* visitor) const override {
      FontResourceClient::Trace(visitor);
    }

   private:
    String DebugName() const override {
      return "CSSFontFaceSrcValue::FontResourceHelper";
    }
  };
  mutable Member<FontResourceHelper> fetched_;
};

template <>
struct DowncastTraits<CSSFontFaceSrcValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsFontFaceSrcValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FACE_SRC_VALUE_H_
