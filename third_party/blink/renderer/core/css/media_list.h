/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2008, 2009, 2010, 2012 Apple Inc. All rights
 * reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_LIST_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/media_query.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CSSRule;
class CSSStyleSheet;
class ExceptionState;
class MediaList;
class MediaQuery;

class CORE_EXPORT MediaQuerySet : public RefCounted<MediaQuerySet> {
 public:
  static scoped_refptr<MediaQuerySet> Create() {
    return base::AdoptRef(new MediaQuerySet());
  }
  static scoped_refptr<MediaQuerySet> Create(const String& media_string);

  bool Set(const String&);
  bool Add(const String&);
  bool Remove(const String&);

  void AddMediaQuery(std::unique_ptr<MediaQuery>);

  const Vector<std::unique_ptr<MediaQuery>>& QueryVector() const {
    return queries_;
  }

  String MediaText() const;

  scoped_refptr<MediaQuerySet> Copy() const {
    return base::AdoptRef(new MediaQuerySet(*this));
  }

 private:
  MediaQuerySet();
  MediaQuerySet(const MediaQuerySet&);

  Vector<std::unique_ptr<MediaQuery>> queries_;
};

class MediaList final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MediaList(scoped_refptr<MediaQuerySet>, CSSStyleSheet* parent_sheet);
  MediaList(scoped_refptr<MediaQuerySet>, CSSRule* parent_rule);

  unsigned length() const { return media_queries_->QueryVector().size(); }
  String item(unsigned index) const;
  void deleteMedium(const String& old_medium, ExceptionState&);
  void appendMedium(const String& new_medium);

  String mediaText() const { return media_queries_->MediaText(); }
  void setMediaText(const String&);

  // Not part of CSSOM.
  CSSRule* ParentRule() const { return parent_rule_; }
  CSSStyleSheet* ParentStyleSheet() const { return parent_style_sheet_; }

  const MediaQuerySet* Queries() const { return media_queries_.get(); }

  void Reattach(scoped_refptr<MediaQuerySet>);

  void Trace(blink::Visitor*) override;

 private:
  scoped_refptr<MediaQuerySet> media_queries_;
  Member<CSSStyleSheet> parent_style_sheet_;
  Member<CSSRule> parent_rule_;
};

}  // namespace blink

#endif
