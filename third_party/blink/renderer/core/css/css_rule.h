/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2007, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_RULE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSParserContext;
class CSSRuleList;
class CSSStyleSheet;
class StyleRuleBase;
enum class SecureContextMode;

class CORE_EXPORT CSSRule : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~CSSRule() override = default;

  // The values must match the table in [1]. See also css_rule.idl.
  // [1] https://wiki.csswg.org/spec/cssom-constants
  enum Type {
    kStyleRule = 1,
    kCharsetRule = 2,
    kImportRule = 3,
    kMediaRule = 4,
    kFontFaceRule = 5,
    kPageRule = 6,
    kKeyframesRule = 7,
    kKeyframeRule = 8,
    kNamespaceRule = 10,
    kSupportsRule = 12,
    kViewportRule = 15,
    kPropertyRule = 18,
    // Experimental features below. Such features must be greater than 1000:
    // the 0-1000 range is reserved by the CSS Working Group.
  };

  virtual Type type() const = 0;
  virtual String cssText() const = 0;
  virtual void Reattach(StyleRuleBase*) = 0;

  virtual CSSRuleList* cssRules() const { return nullptr; }

  void SetParentStyleSheet(CSSStyleSheet*);

  void SetParentRule(CSSRule*);

  void Trace(blink::Visitor*) override;

  CSSStyleSheet* parentStyleSheet() const {
    if (parent_is_rule_)
      return parent_rule_ ? parent_rule_->parentStyleSheet() : nullptr;
    return parent_style_sheet_;
  }

  CSSRule* parentRule() const {
    return parent_is_rule_ ? parent_rule_ : nullptr;
  }

  // The CSSOM spec states that "setting the cssText attribute must do nothing."
  void setCSSText(const String&) {}

 protected:
  CSSRule(CSSStyleSheet* parent)
      : has_cached_selector_text_(false),
        parent_is_rule_(false),
        parent_style_sheet_(parent) {}

  bool HasCachedSelectorText() const { return has_cached_selector_text_; }
  void SetHasCachedSelectorText(bool has_cached_selector_text) const {
    has_cached_selector_text_ = has_cached_selector_text;
  }

  const CSSParserContext* ParserContext(SecureContextMode) const;

 private:
  mutable unsigned char has_cached_selector_text_ : 1;
  unsigned char parent_is_rule_ : 1;

  // These should be Members, but no Members in unions.
  union {
    CSSRule* parent_rule_;
    CSSStyleSheet* parent_style_sheet_;
  };
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_RULE_H_
