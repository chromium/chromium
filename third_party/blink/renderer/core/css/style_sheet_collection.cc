/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/style_sheet_collection.h"

#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/rule_set.h"

namespace blink {

StyleSheetCollection::StyleSheetCollection() = default;

void StyleSheetCollection::Dispose() {
  style_sheets_for_style_sheet_list_.clear();
  active_author_style_sheets_.clear();
}

void StyleSheetCollection::Swap(StyleSheetCollection& other) {
  swap(style_sheets_for_style_sheet_list_,
       other.style_sheets_for_style_sheet_list_);
  active_author_style_sheets_.swap(other.active_author_style_sheets_);
  sheet_list_dirty_ = false;
}

void StyleSheetCollection::SwapSheetsForSheetList(
    HeapVector<Member<StyleSheet>>& sheets) {
  swap(style_sheets_for_style_sheet_list_, sheets);
  sheet_list_dirty_ = false;
}

void StyleSheetCollection::AppendActiveStyleSheet(
    const ActiveStyleSheet& active_sheet) {
  active_author_style_sheets_.push_back(active_sheet);
}

void StyleSheetCollection::AppendSheetForList(StyleSheet* sheet) {
  style_sheets_for_style_sheet_list_.push_back(sheet);
}

void StyleSheetCollection::Trace(blink::Visitor* visitor) {
  visitor->Trace(active_author_style_sheets_);
  visitor->Trace(style_sheets_for_style_sheet_list_);
}

}  // namespace blink
