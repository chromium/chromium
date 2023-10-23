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
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_origin_clean.h"
#include "third_party/blink/renderer/core/css/css_url_data.h"
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
  static CSSFontFaceSrcValue* Create(CSSUrlData url_data,
                                     const Referrer& referrer,
                                     scoped_refptr<const DOMWrapperWorld> world,
                                     OriginClean origin_clean,
                                     bool is_ad_related) {
    return MakeGarbageCollected<CSSFontFaceSrcValue>(
        std::move(url_data), referrer, false, std::move(world), origin_clean,
        is_ad_related);
  }
  static CSSFontFaceSrcValue* CreateLocal(
      const String& absolute_resource,
      scoped_refptr<const DOMWrapperWorld> world,
      OriginClean origin_clean,
      bool is_ad_related) {
    return MakeGarbageCollected<CSSFontFaceSrcValue>(
        CSSUrlData(AtomicString(absolute_resource), KURL()), Referrer(), true,
        std::move(world), origin_clean, is_ad_related);
  }

  CSSFontFaceSrcValue(CSSUrlData url_data,
                      const Referrer& referrer,
                      bool local,
                      scoped_refptr<const DOMWrapperWorld> world,
                      OriginClean origin_clean,
                      bool is_ad_related)
      : CSSValue(kFontFaceSrcClass),
        url_data_(std::move(url_data)),
        referrer_(referrer),
        world_(std::move(world)),
        is_local_(local),
        origin_clean_(origin_clean),
        is_ad_related_(is_ad_related) {}

  // Returns the local() resource name. Only usable if IsLocal() returns true.
  const String& LocalResource() const { return url_data_.UnresolvedUrl(); }
  bool IsLocal() const { return is_local_; }

  /* Format is serialized as string, so we can set this to string internally. It
   * does not affect functionality downstream - i.e. the font face is handled
   * the same way whatsoever, if the format is supported. */
  void SetFormat(const String& format) { format_ = format; }

  /* Only supported technologies need to be listed here, as we can reject other
   * font face source component values, hence remove SVG and incremental for
   * now, compare https://drafts.csswg.org/css-fonts-4/#font-face-src-parsing */
  enum class FontTechnology {
    kTechnologyFeaturesAAT,
    kTechnologyFeaturesOT,
    kTechnologyCOLRv0,
    kTechnologyCOLRv1,
    kTechnologySBIX,
    kTechnologyCDBT,
    kTechnologyVariations,
    kTechnologyPalettes,
    kTechnologyUnknown
  };
  void AppendTechnology(FontTechnology technology);

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

  Vector<FontTechnology> technologies_;
  CSSUrlData url_data_;
  String format_;
  const Referrer referrer_;
  const scoped_refptr<const DOMWrapperWorld> world_;
  mutable Member<FontResource> fetched_;
  const bool is_local_;
  const OriginClean origin_clean_;
  bool is_ad_related_;
};

template <>
struct DowncastTraits<CSSFontFaceSrcValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsFontFaceSrcValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FACE_SRC_VALUE_H_
