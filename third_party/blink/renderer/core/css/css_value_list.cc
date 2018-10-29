/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/css_value_list.h"

#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

struct SameSizeAsCSSValueList : CSSValue {
  Vector<Member<CSSValue>, 4> list_values;
};
ASSERT_SIZE(CSSValueList, SameSizeAsCSSValueList);

CSSValueList::CSSValueList(ClassType class_type,
                           ValueListSeparator list_separator)
    : CSSValue(class_type) {
  value_list_separator_ = list_separator;
}

CSSValueList::CSSValueList(ValueListSeparator list_separator)
    : CSSValue(kValueListClass) {
  value_list_separator_ = list_separator;
}

bool CSSValueList::RemoveAll(const CSSValue& val) {
  bool found = false;
  for (int index = values_.size() - 1; index >= 0; --index) {
    Member<const CSSValue>& value = values_.at(index);
    if (value && *value == val) {
      values_.EraseAt(index);
      found = true;
    }
  }

  return found;
}

bool CSSValueList::HasValue(const CSSValue& val) const {
  for (wtf_size_t index = 0; index < values_.size(); index++) {
    const Member<const CSSValue>& value = values_.at(index);
    if (value && *value == val) {
      return true;
    }
  }
  return false;
}

CSSValueList* CSSValueList::Copy() const {
  CSSValueList* new_list = nullptr;
  switch (value_list_separator_) {
    case kSpaceSeparator:
      new_list = CreateSpaceSeparated();
      break;
    case kCommaSeparator:
      new_list = CreateCommaSeparated();
      break;
    case kSlashSeparator:
      new_list = CreateSlashSeparated();
      break;
    default:
      NOTREACHED();
  }
  new_list->values_ = values_;
  return new_list;
}

String CSSValueList::CustomCSSText() const {
  StringBuilder result;
  String separator;
  switch (value_list_separator_) {
    case kSpaceSeparator:
      separator = " ";
      break;
    case kCommaSeparator:
      separator = ", ";
      break;
    case kSlashSeparator:
      separator = " / ";
      break;
    default:
      NOTREACHED();
  }

  unsigned size = values_.size();
  for (unsigned i = 0; i < size; i++) {
    if (!result.IsEmpty())
      result.Append(separator);
    result.Append(values_[i]->CssText());
  }

  return result.ToString();
}

bool CSSValueList::Equals(const CSSValueList& other) const {
  return value_list_separator_ == other.value_list_separator_ &&
         CompareCSSValueVector(values_, other.values_);
}

bool CSSValueList::HasFailedOrCanceledSubresources() const {
  for (unsigned i = 0; i < values_.size(); ++i) {
    if (values_[i]->HasFailedOrCanceledSubresources())
      return true;
  }
  return false;
}

bool CSSValueList::MayContainUrl() const {
  for (const auto& value : values_) {
    if (value->MayContainUrl())
      return true;
  }
  return false;
}

void CSSValueList::ReResolveUrl(const Document& document) const {
  for (const auto& value : values_)
    value->ReResolveUrl(document);
}

void CSSValueList::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(values_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
