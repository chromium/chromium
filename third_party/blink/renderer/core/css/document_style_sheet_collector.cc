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

#include "third_party/blink/renderer/core/css/document_style_sheet_collector.h"

#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/document_style_sheet_collection.h"
#include "third_party/blink/renderer/core/css/style_sheet.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

DocumentStyleSheetCollector::DocumentStyleSheetCollector(
    StyleSheetCollection* collection,
    HeapVector<Member<StyleSheet>>* sheets_for_list)
    : collection_(collection),
      style_sheets_for_style_sheet_list_(sheets_for_list) {}

void DocumentStyleSheetCollector::AppendActiveStyleSheet(
    const ActiveStyleSheet& sheet) {
  DCHECK(collection_);
  collection_->AppendActiveStyleSheet(sheet);
}

void DocumentStyleSheetCollector::AppendSheetForList(StyleSheet* sheet) {
  if (style_sheets_for_style_sheet_list_) {
    style_sheets_for_style_sheet_list_->push_back(sheet);
  } else {
    collection_->AppendSheetForList(sheet);
  }
}

void DocumentStyleSheetCollector::AppendRuleSetDiff(RuleSetDiff* diff) {
  collection_->AppendRuleSetDiff(diff);
}

ActiveDocumentStyleSheetCollector::ActiveDocumentStyleSheetCollector(
    StyleSheetCollection& collection)
    : DocumentStyleSheetCollector(&collection, nullptr) {}

ImportedDocumentStyleSheetCollector::ImportedDocumentStyleSheetCollector(
    DocumentStyleSheetCollector& collector,
    HeapVector<Member<StyleSheet>>& sheet_for_list)
    : DocumentStyleSheetCollector(collector.collection_, &sheet_for_list) {}

}  // namespace blink
