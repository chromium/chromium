/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2010, 2011 Apple Inc. All rights
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

#include "third_party/blink/renderer/core/html/html_table_element.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_table_caption_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html/html_table_rows_collection.h"
#include "third_party/blink/renderer/core/html/html_table_section_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

HTMLTableElement::HTMLTableElement(Document& document)
    : HTMLElement(html_names::kTableTag, document),
      border_attr_(false),
      border_color_attr_(false),
      frame_attr_(false),
      rules_attr_(kUnsetRules),
      padding_(1) {}

// An explicit empty destructor should be in html_table_element.cc, because
// if an implicit destructor is used or an empty destructor is defined in
// html_table_element.h, when including html_table_element.h, msvc tries to
// expand the destructor and causes a compile error because of lack of
// CSSPropertyValueSet definition.
HTMLTableElement::~HTMLTableElement() = default;

HTMLTableCaptionElement* HTMLTableElement::caption() const {
  return Traversal<HTMLTableCaptionElement>::FirstChild(*this);
}

void HTMLTableElement::setCaption(HTMLTableCaptionElement* new_caption,
                                  ExceptionState& exception_state) {
  deleteCaption();
  if (new_caption)
    InsertBefore(new_caption, firstChild(), exception_state);
}

HTMLTableSectionElement* HTMLTableElement::tHead() const {
  return To<HTMLTableSectionElement>(Traversal<HTMLElement>::FirstChild(
      *this, HasHTMLTagName(html_names::kTheadTag)));
}

void HTMLTableElement::setTHead(HTMLTableSectionElement* new_head,
                                ExceptionState& exception_state) {
  if (new_head && !new_head->HasTagName(html_names::kTheadTag)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kHierarchyRequestError,
                                      "Not a thead element.");
    return;
  }

  deleteTHead();
  if (!new_head)
    return;

  HTMLElement* child;
  for (child = Traversal<HTMLElement>::FirstChild(*this); child;
       child = Traversal<HTMLElement>::NextSibling(*child)) {
    if (!child->HasTagName(html_names::kCaptionTag) &&
        !child->HasTagName(html_names::kColgroupTag))
      break;
  }

  InsertBefore(new_head, child, exception_state);
}

HTMLTableSectionElement* HTMLTableElement::tFoot() const {
  return To<HTMLTableSectionElement>(Traversal<HTMLElement>::FirstChild(
      *this, HasHTMLTagName(html_names::kTfootTag)));
}

void HTMLTableElement::setTFoot(HTMLTableSectionElement* new_foot,
                                ExceptionState& exception_state) {
  if (new_foot && !new_foot->HasTagName(html_names::kTfootTag)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kHierarchyRequestError,
                                      "Not a tfoot element.");
    return;
  }

  deleteTFoot();

  if (new_foot)
    AppendChild(new_foot, exception_state);
}

HTMLTableSectionElement* HTMLTableElement::createTHead() {
  if (HTMLTableSectionElement* existing_head = tHead())
    return existing_head;
  auto* head = MakeGarbageCollected<HTMLTableSectionElement>(
      html_names::kTheadTag, GetDocument());
  setTHead(head, IGNORE_EXCEPTION_FOR_TESTING);
  return head;
}

void HTMLTableElement::deleteTHead() {
  RemoveChild(tHead(), IGNORE_EXCEPTION_FOR_TESTING);
}

HTMLTableSectionElement* HTMLTableElement::createTFoot() {
  if (HTMLTableSectionElement* existing_foot = tFoot())
    return existing_foot;
  auto* foot = MakeGarbageCollected<HTMLTableSectionElement>(
      html_names::kTfootTag, GetDocument());
  setTFoot(foot, IGNORE_EXCEPTION_FOR_TESTING);
  return foot;
}

void HTMLTableElement::deleteTFoot() {
  RemoveChild(tFoot(), IGNORE_EXCEPTION_FOR_TESTING);
}

HTMLTableSectionElement* HTMLTableElement::createTBody() {
  auto* body = MakeGarbageCollected<HTMLTableSectionElement>(
      html_names::kTbodyTag, GetDocument());
  Node* reference_element = LastBody() ? LastBody()->nextSibling() : nullptr;

  InsertBefore(body, reference_element);
  return body;
}

HTMLTableCaptionElement* HTMLTableElement::createCaption() {
  if (HTMLTableCaptionElement* existing_caption = caption())
    return existing_caption;
  auto* caption = MakeGarbageCollected<HTMLTableCaptionElement>(GetDocument());
  setCaption(caption, IGNORE_EXCEPTION_FOR_TESTING);
  return caption;
}

void HTMLTableElement::deleteCaption() {
  RemoveChild(caption(), IGNORE_EXCEPTION_FOR_TESTING);
}

HTMLTableSectionElement* HTMLTableElement::LastBody() const {
  return To<HTMLTableSectionElement>(Traversal<HTMLElement>::LastChild(
      *this, HasHTMLTagName(html_names::kTbodyTag)));
}

HTMLTableRowElement* HTMLTableElement::insertRow(
    int index,
    ExceptionState& exception_state) {
  if (index < -1) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The index provided (" + String::Number(index) + ") is less than -1.");
    return nullptr;
  }

  HTMLTableRowElement* last_row = nullptr;
  HTMLTableRowElement* row = nullptr;
  if (index == -1) {
    last_row = HTMLTableRowsCollection::LastRow(*this);
  } else {
    for (int i = 0; i <= index; ++i) {
      row = HTMLTableRowsCollection::RowAfter(*this, last_row);
      if (!row) {
        if (i != index) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kIndexSizeError,
              "The index provided (" + String::Number(index) +
                  ") is greater than the number of rows in the table (" +
                  String::Number(i) + ").");
          return nullptr;
        }
        break;
      }
      last_row = row;
    }
  }

  ContainerNode* parent;
  if (last_row) {
    parent = row ? row->parentNode() : last_row->parentNode();
  } else {
    parent = LastBody();
    if (!parent) {
      auto* new_body = MakeGarbageCollected<HTMLTableSectionElement>(
          html_names::kTbodyTag, GetDocument());
      auto* new_row = MakeGarbageCollected<HTMLTableRowElement>(GetDocument());
      new_body->AppendChild(new_row, exception_state);
      AppendChild(new_body, exception_state);
      return new_row;
    }
  }

  auto* new_row = MakeGarbageCollected<HTMLTableRowElement>(GetDocument());
  parent->InsertBefore(new_row, row, exception_state);
  return new_row;
}

void HTMLTableElement::deleteRow(int index, ExceptionState& exception_state) {
  if (index < -1) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The index provided (" + String::Number(index) + ") is less than -1.");
    return;
  }

  HTMLTableRowElement* row = nullptr;
  int i = 0;
  if (index == -1) {
    row = HTMLTableRowsCollection::LastRow(*this);
    if (!row)
      return;
  } else {
    for (i = 0; i <= index; ++i) {
      row = HTMLTableRowsCollection::RowAfter(*this, row);
      if (!row)
        break;
    }
  }
  if (!row) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The index provided (" + String::Number(index) +
            ") is greater than the number of rows in the table (" +
            String::Number(i) + ").");
    return;
  }
  row->remove(exception_state);
}

void HTMLTableElement::SetNeedsTableStyleRecalc() const {
  Element* element = ElementTraversal::Next(*this, this);
  while (element) {
    element->SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::FromAttribute(html_names::kRulesAttr));
    if (IsA<HTMLTableCellElement>(*element))
      element = ElementTraversal::NextSkippingChildren(*element, this);
    else
      element = ElementTraversal::Next(*element, this);
  }
}

static bool GetBordersFromFrameAttributeValue(const AtomicString& value,
                                              bool& border_top,
                                              bool& border_right,
                                              bool& border_bottom,
                                              bool& border_left) {
  border_top = false;
  border_right = false;
  border_bottom = false;
  border_left = false;

  if (EqualIgnoringASCIICase(value, "above"))
    border_top = true;
  else if (EqualIgnoringASCIICase(value, "below"))
    border_bottom = true;
  else if (EqualIgnoringASCIICase(value, "hsides"))
    border_top = border_bottom = true;
  else if (EqualIgnoringASCIICase(value, "vsides"))
    border_left = border_right = true;
  else if (EqualIgnoringASCIICase(value, "lhs"))
    border_left = true;
  else if (EqualIgnoringASCIICase(value, "rhs"))
    border_right = true;
  else if (EqualIgnoringASCIICase(value, "box") ||
           EqualIgnoringASCIICase(value, "border"))
    border_top = border_bottom = border_left = border_right = true;
  else if (!EqualIgnoringASCIICase(value, "void"))
    return false;
  return true;
}

void HTMLTableElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kWidthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kWidth, value,
                         kAllowPercentageValues, kDontAllowZeroValues);
  } else if (name == html_names::kHeightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kHeight, value);
  } else if (name == html_names::kBorderAttr) {
    unsigned width = ParseBorderWidthAttribute(value);
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kBorderTopWidth, width,
        CSSPrimitiveValue::UnitType::kPixels);
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kBorderBottomWidth, width,
        CSSPrimitiveValue::UnitType::kPixels);
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kBorderLeftWidth, width,
        CSSPrimitiveValue::UnitType::kPixels);
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kBorderRightWidth, width,
        CSSPrimitiveValue::UnitType::kPixels);
  } else if (name == html_names::kBordercolorAttr) {
    if (!value.empty())
      AddHTMLColorToStyle(style, CSSPropertyID::kBorderColor, value);
  } else if (name == html_names::kBgcolorAttr) {
    AddHTMLColorToStyle(style, CSSPropertyID::kBackgroundColor, value);
  } else if (name == html_names::kBackgroundAttr) {
    AddHTMLBackgroundImageToStyle(style, value);
  } else if (name == html_names::kValignAttr) {
    if (!value.empty()) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kVerticalAlign, value);
    }
  } else if (name == html_names::kCellspacingAttr) {
    if (!value.empty()) {
      for (CSSPropertyID property_id :
           {CSSPropertyID::kWebkitBorderHorizontalSpacing,
            CSSPropertyID::kWebkitBorderVerticalSpacing}) {
        AddHTMLLengthToStyle(style, property_id, value,
                             kDontAllowPercentageValues);
      }
    }
  } else if (name == html_names::kAlignAttr) {
    if (!value.empty()) {
      if (EqualIgnoringASCIICase(value, "center")) {
        AddPropertyToPresentationAttributeStyle(
            style, CSSPropertyID::kMarginInlineStart, CSSValueID::kAuto);
        AddPropertyToPresentationAttributeStyle(
            style, CSSPropertyID::kMarginInlineEnd, CSSValueID::kAuto);
      } else {
        AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kFloat,
                                                value);
      }
    }
  } else if (name == html_names::kRulesAttr) {
    // The presence of a valid rules attribute causes border collapsing to be
    // enabled.
    if (rules_attr_ != kUnsetRules) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kBorderCollapse, CSSValueID::kCollapse);
    }
  } else if (name == html_names::kFrameAttr) {
    bool border_top;
    bool border_right;
    bool border_bottom;
    bool border_left;
    if (GetBordersFromFrameAttributeValue(value, border_top, border_right,
                                          border_bottom, border_left)) {
      for (CSSPropertyID property_id :
           {CSSPropertyID::kBorderTopWidth, CSSPropertyID::kBorderBottomWidth,
            CSSPropertyID::kBorderLeftWidth,
            CSSPropertyID::kBorderRightWidth}) {
        AddPropertyToPresentationAttributeStyle(style, property_id,
                                                CSSValueID::kThin);
      }
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kBorderTopStyle,
          border_top ? CSSValueID::kSolid : CSSValueID::kHidden);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kBorderBottomStyle,
          border_bottom ? CSSValueID::kSolid : CSSValueID::kHidden);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kBorderLeftStyle,
          border_left ? CSSValueID::kSolid : CSSValueID::kHidden);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kBorderRightStyle,
          border_right ? CSSValueID::kSolid : CSSValueID::kHidden);
    }
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

bool HTMLTableElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kWidthAttr || name == html_names::kHeightAttr ||
      name == html_names::kBgcolorAttr || name == html_names::kBackgroundAttr ||
      name == html_names::kValignAttr || name == html_names::kVspaceAttr ||
      name == html_names::kHspaceAttr || name == html_names::kAlignAttr ||
      name == html_names::kCellspacingAttr || name == html_names::kBorderAttr ||
      name == html_names::kBordercolorAttr || name == html_names::kFrameAttr ||
      name == html_names::kRulesAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLTableElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  CellBorders borders_before = GetCellBorders();
  uint16_t old_padding = padding_;

  if (name == html_names::kBorderAttr) {
    // FIXME: This attribute is a mess.
    border_attr_ = ParseBorderWidthAttribute(params.new_value);
  } else if (name == html_names::kBordercolorAttr) {
    border_color_attr_ = !params.new_value.empty();
  } else if (name == html_names::kFrameAttr) {
    // FIXME: This attribute is a mess.
    bool border_top;
    bool border_right;
    bool border_bottom;
    bool border_left;
    frame_attr_ = GetBordersFromFrameAttributeValue(
        params.new_value, border_top, border_right, border_bottom, border_left);
  } else if (name == html_names::kRulesAttr) {
    rules_attr_ = kUnsetRules;
    if (EqualIgnoringASCIICase(params.new_value, "none"))
      rules_attr_ = kNoneRules;
    else if (EqualIgnoringASCIICase(params.new_value, "groups"))
      rules_attr_ = kGroupsRules;
    else if (EqualIgnoringASCIICase(params.new_value, "rows"))
      rules_attr_ = kRowsRules;
    else if (EqualIgnoringASCIICase(params.new_value, "cols"))
      rules_attr_ = kColsRules;
    else if (EqualIgnoringASCIICase(params.new_value, "all"))
      rules_attr_ = kAllRules;
  } else if (params.name == html_names::kCellpaddingAttr) {
    if (!params.new_value.empty()) {
      padding_ =
          std::max(0, std::min((int32_t)std::numeric_limits<uint16_t>::max(),
                               params.new_value.ToInt()));
    } else {
      padding_ = 1;
    }
  } else if (params.name == html_names::kColsAttr) {
    // ###
  } else {
    HTMLElement::ParseAttribute(params);
  }

  if (borders_before != GetCellBorders() || old_padding != padding_) {
    shared_cell_style_ = nullptr;
    SetNeedsTableStyleRecalc();
  }
}

static CSSPropertyValueSet* CreateBorderStyle(CSSValueID value) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  style->SetLonghandProperty(CSSPropertyID::kBorderTopStyle, value);
  style->SetLonghandProperty(CSSPropertyID::kBorderBottomStyle, value);
  style->SetLonghandProperty(CSSPropertyID::kBorderLeftStyle, value);
  style->SetLonghandProperty(CSSPropertyID::kBorderRightStyle, value);
  return style;
}

const CSSPropertyValueSet*
HTMLTableElement::AdditionalPresentationAttributeStyle() {
  if (frame_attr_)
    return nullptr;

  if (!border_attr_ && !border_color_attr_) {
    // Setting the border to 'hidden' allows it to win over any border
    // set on the table's cells during border-conflict resolution.
    if (rules_attr_ != kUnsetRules) {
      DEFINE_STATIC_LOCAL(Persistent<CSSPropertyValueSet>, solid_border_style,
                          (CreateBorderStyle(CSSValueID::kHidden)));
      return solid_border_style;
    }
    return nullptr;
  }

  if (border_color_attr_) {
    DEFINE_STATIC_LOCAL(Persistent<CSSPropertyValueSet>, solid_border_style,
                        (CreateBorderStyle(CSSValueID::kSolid)));
    return solid_border_style;
  }
  DEFINE_STATIC_LOCAL(Persistent<CSSPropertyValueSet>, outset_border_style,
                      (CreateBorderStyle(CSSValueID::kOutset)));
  return outset_border_style;
}

HTMLTableElement::CellBorders HTMLTableElement::GetCellBorders() const {
  switch (rules_attr_) {
    case kNoneRules:
    case kGroupsRules:
      return kNoBorders;
    case kAllRules:
      return kSolidBorders;
    case kColsRules:
      return kSolidBordersColsOnly;
    case kRowsRules:
      return kSolidBordersRowsOnly;
    case kUnsetRules:
      if (!border_attr_)
        return kNoBorders;
      if (border_color_attr_)
        return kSolidBorders;
      return kInsetBorders;
  }
  NOTREACHED_IN_MIGRATION();
  return kNoBorders;
}

CSSPropertyValueSet* HTMLTableElement::CreateSharedCellStyle() {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);

  switch (GetCellBorders()) {
    case kSolidBordersColsOnly:
      style->SetLonghandProperty(CSSPropertyID::kBorderLeftWidth,
                                 CSSValueID::kThin);
      style->SetLonghandProperty(CSSPropertyID::kBorderRightWidth,
                                 CSSValueID::kThin);
      style->SetLonghandProperty(CSSPropertyID::kBorderLeftStyle,
                                 CSSValueID::kSolid);
      style->SetLonghandProperty(CSSPropertyID::kBorderRightStyle,
                                 CSSValueID::kSolid);
      style->SetProperty(CSSPropertyID::kBorderColor,
                         *CSSInheritedValue::Create());
      break;
    case kSolidBordersRowsOnly:
      style->SetLonghandProperty(CSSPropertyID::kBorderTopWidth,
                                 CSSValueID::kThin);
      style->SetLonghandProperty(CSSPropertyID::kBorderBottomWidth,
                                 CSSValueID::kThin);
      style->SetLonghandProperty(CSSPropertyID::kBorderTopStyle,
                                 CSSValueID::kSolid);
      style->SetLonghandProperty(CSSPropertyID::kBorderBottomStyle,
                                 CSSValueID::kSolid);
      style->SetProperty(CSSPropertyID::kBorderColor,
                         *CSSInheritedValue::Create());
      break;
    case kSolidBorders:
      style->SetProperty(CSSPropertyID::kBorderWidth,
                         *CSSNumericLiteralValue::Create(
                             1, CSSPrimitiveValue::UnitType::kPixels));
      style->SetProperty(CSSPropertyID::kBorderStyle,
                         *CSSIdentifierValue::Create(CSSValueID::kSolid));
      style->SetProperty(CSSPropertyID::kBorderColor,
                         *CSSInheritedValue::Create());
      break;
    case kInsetBorders:
      style->SetProperty(CSSPropertyID::kBorderWidth,
                         *CSSNumericLiteralValue::Create(
                             1, CSSPrimitiveValue::UnitType::kPixels));
      style->SetProperty(CSSPropertyID::kBorderStyle,
                         *CSSIdentifierValue::Create(CSSValueID::kInset));
      style->SetProperty(CSSPropertyID::kBorderColor,
                         *CSSInheritedValue::Create());
      break;
    case kNoBorders:
      // If 'rules=none' then allow any borders set at cell level to take
      // effect.
      break;
  }

  if (padding_)
    style->SetProperty(CSSPropertyID::kPadding,
                       *CSSNumericLiteralValue::Create(
                           padding_, CSSPrimitiveValue::UnitType::kPixels));

  return style;
}

const CSSPropertyValueSet* HTMLTableElement::AdditionalCellStyle() {
  if (!shared_cell_style_)
    shared_cell_style_ = CreateSharedCellStyle();
  return shared_cell_style_.Get();
}

static CSSPropertyValueSet* CreateGroupBorderStyle(int rows) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  if (rows) {
    style->SetLonghandProperty(CSSPropertyID::kBorderTopWidth,
                               CSSValueID::kThin);
    style->SetLonghandProperty(CSSPropertyID::kBorderBottomWidth,
                               CSSValueID::kThin);
    style->SetLonghandProperty(CSSPropertyID::kBorderTopStyle,
                               CSSValueID::kSolid);
    style->SetLonghandProperty(CSSPropertyID::kBorderBottomStyle,
                               CSSValueID::kSolid);
  } else {
    style->SetLonghandProperty(CSSPropertyID::kBorderLeftWidth,
                               CSSValueID::kThin);
    style->SetLonghandProperty(CSSPropertyID::kBorderRightWidth,
                               CSSValueID::kThin);
    style->SetLonghandProperty(CSSPropertyID::kBorderLeftStyle,
                               CSSValueID::kSolid);
    style->SetLonghandProperty(CSSPropertyID::kBorderRightStyle,
                               CSSValueID::kSolid);
  }
  return style;
}

const CSSPropertyValueSet* HTMLTableElement::AdditionalGroupStyle(bool rows) {
  if (rules_attr_ != kGroupsRules)
    return nullptr;

  if (rows) {
    DEFINE_STATIC_LOCAL(Persistent<CSSPropertyValueSet>, row_border_style,
                        (CreateGroupBorderStyle(true)));
    return row_border_style;
  }
  DEFINE_STATIC_LOCAL(Persistent<CSSPropertyValueSet>, column_border_style,
                      (CreateGroupBorderStyle(false)));
  return column_border_style;
}

bool HTMLTableElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kBackgroundAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLTableElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == html_names::kBackgroundAttr ||
         HTMLElement::HasLegalLinkAttribute(name);
}

HTMLTableRowsCollection* HTMLTableElement::rows() {
  return EnsureCachedCollection<HTMLTableRowsCollection>(kTableRows);
}

HTMLCollection* HTMLTableElement::tBodies() {
  return EnsureCachedCollection<HTMLCollection>(kTableTBodies);
}

const AtomicString& HTMLTableElement::Rules() const {
  return FastGetAttribute(html_names::kRulesAttr);
}

const AtomicString& HTMLTableElement::Summary() const {
  return FastGetAttribute(html_names::kSummaryAttr);
}

void HTMLTableElement::Trace(Visitor* visitor) const {
  visitor->Trace(shared_cell_style_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
