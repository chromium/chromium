// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/lcp_critical_path_predictor/element_locator.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::element_locator {

absl::optional<ElementLocator> OfElement(Element* element) {
  ElementLocator locator;

  while (element) {
    Element* parent = element->parentElement();

    if (element->HasID()) {
      // Peg on element id if that exists

      ElementLocator_Component_Id* id_comp =
          locator.add_components()->mutable_id();
      id_comp->set_id_attr(element->GetIdAttribute().Utf8());
      break;
    } else if (parent) {
      // Last resort: n-th element that has the `tag_name`.

      AtomicString tag_name = element->localName();

      int nth = 0;
      for (Node* sibling = parent->firstChild(); sibling;
           sibling = sibling->nextSibling()) {
        Element* sibling_el = DynamicTo<Element>(sibling);
        if (!sibling_el || sibling_el->localName() != tag_name) {
          continue;
        }

        if (sibling_el == element) {
          ElementLocator_Component_NthTagName* nth_comp =
              locator.add_components()->mutable_nth();
          nth_comp->set_tag_name(tag_name.Utf8());
          nth_comp->set_index(nth);
          break;
        }

        ++nth;
      }
    }

    element = parent;
  }

  return locator;
}

String ToString(const ElementLocator& locator) {
  StringBuilder builder;

  for (const auto& c : locator.components()) {
    builder.Append('/');
    if (c.has_id()) {
      builder.Append('#');
      builder.Append(c.id().id_attr().c_str());
    } else if (c.has_nth()) {
      builder.Append(c.nth().tag_name().c_str());
      builder.Append('[');
      builder.AppendNumber(c.nth().index());
      builder.Append(']');
    } else {
      builder.Append("unknown_type");
    }
  }

  return builder.ReleaseString();
}

void HTMLStackItem::IncrementChildrenCount(
    const AtomicString& children_tag_name) {
  auto add_result = children_counts.insert(children_tag_name, 1);
  if (!add_result.is_new_entry) {
    ++add_result.stored_value->value;
  }
}

TokenStreamMatcher::TokenStreamMatcher(Vector<ElementLocator> locators)
    : locators_(locators) {}

TokenStreamMatcher::~TokenStreamMatcher() = default;

namespace {

// Set of element tag names that needs to run a "close a p element" step in
// https://html.spec.whatwg.org/multipage/parsing.html#parsing-main-inbody
const HashSet<AtomicString>& ClosePElementSet() {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, s_close_p_element_set, ());
  if (s_close_p_element_set.empty()) {
    s_close_p_element_set.insert(html_names::kAddressTag.LocalName());
    s_close_p_element_set.insert(html_names::kArticleTag.LocalName());
    s_close_p_element_set.insert(html_names::kAsideTag.LocalName());
    s_close_p_element_set.insert(html_names::kBlockquoteTag.LocalName());
    s_close_p_element_set.insert(html_names::kCenterTag.LocalName());
    s_close_p_element_set.insert(html_names::kDetailsTag.LocalName());
    s_close_p_element_set.insert(html_names::kDirTag.LocalName());
    s_close_p_element_set.insert(html_names::kDivTag.LocalName());
    s_close_p_element_set.insert(html_names::kDlTag.LocalName());
    s_close_p_element_set.insert(html_names::kFieldsetTag.LocalName());
    s_close_p_element_set.insert(html_names::kFigcaptionTag.LocalName());
    s_close_p_element_set.insert(html_names::kFigureTag.LocalName());
    s_close_p_element_set.insert(html_names::kFooterTag.LocalName());
    s_close_p_element_set.insert(html_names::kHeaderTag.LocalName());
    s_close_p_element_set.insert(html_names::kHgroupTag.LocalName());
    s_close_p_element_set.insert(html_names::kMainTag.LocalName());
    s_close_p_element_set.insert(html_names::kMenuTag.LocalName());
    s_close_p_element_set.insert(html_names::kNavTag.LocalName());
    s_close_p_element_set.insert(html_names::kOlTag.LocalName());
    s_close_p_element_set.insert(html_names::kPTag.LocalName());

    // The spec says that we should run the step for the "search" tag as well,
    // but we don't have the implementation in Blink yet.
    // s_close_p_element_set.insert(html_names::kSearchTag.LocalName());

    s_close_p_element_set.insert(html_names::kSectionTag.LocalName());
    s_close_p_element_set.insert(html_names::kSummaryTag.LocalName());
    s_close_p_element_set.insert(html_names::kUlTag.LocalName());

    s_close_p_element_set.insert(html_names::kH1Tag.LocalName());
    s_close_p_element_set.insert(html_names::kH2Tag.LocalName());
    s_close_p_element_set.insert(html_names::kH3Tag.LocalName());
    s_close_p_element_set.insert(html_names::kH4Tag.LocalName());
    s_close_p_element_set.insert(html_names::kH5Tag.LocalName());
    s_close_p_element_set.insert(html_names::kH6Tag.LocalName());

    s_close_p_element_set.insert(html_names::kPreTag.LocalName());
    s_close_p_element_set.insert(html_names::kListingTag.LocalName());
    s_close_p_element_set.insert(html_names::kFormTag.LocalName());
    s_close_p_element_set.insert(html_names::kPlaintextTag.LocalName());

    s_close_p_element_set.insert(html_names::kXmpTag.LocalName());
  }
  return s_close_p_element_set;
}

// The list of tags that their start tag tokens need to be closed immediately,
// with the following spec text:
// <spec>Insert an HTML element for the token. Immediately pop the current node
// off the stack of open elements.</spec>
const HashSet<AtomicString>& ImmediatelyPopTheCurrentNodeTags() {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, set, ());
  if (set.empty()) {
    set.insert(html_names::kAreaTag.LocalName());
    set.insert(html_names::kBrTag.LocalName());
    set.insert(html_names::kEmbedTag.LocalName());
    set.insert(html_names::kImgTag.LocalName());
    set.insert(html_names::kKeygenTag.LocalName());
    set.insert(html_names::kWbrTag.LocalName());
    set.insert(html_names::kInputTag.LocalName());
    set.insert(html_names::kParamTag.LocalName());
    set.insert(html_names::kSourceTag.LocalName());
    set.insert(html_names::kTrackTag.LocalName());
    set.insert(html_names::kHrTag.LocalName());
  }
  return set;
}

bool MatchLocator(const ElementLocator& locator,
                  base::span<const HTMLStackItem> stack) {
  if (locator.components_size() == 0) {
    return false;
  }

  for (const auto& c : locator.components()) {
    // Note: we check `stack.size() < 2` since there is a sentinel value at
    //       `stack[0]`, and we would like to check if we have non-sentinel
    //       stack items.
    if (stack.size() < 2) {
      return false;
    }

    const HTMLStackItem& matched_item = stack.back();
    stack = stack.first(stack.size() - 1);
    const HTMLStackItem& parent_item = stack.back();

    switch (c.component_case()) {
      case ElementLocator_Component::kId:
        if (matched_item.id_attr.Utf8() != c.id().id_attr()) {
          return false;
        }
        break;

      case ElementLocator_Component::kNth: {
        AtomicString tag_name(c.nth().tag_name().c_str());

        // Check if tag_name matches
        if (matched_item.tag_name.Utf8() != c.nth().tag_name()) {
          return false;
        }

        // Check if the element is actually the nth
        // child of its parent.
        auto it = parent_item.children_counts.find(tag_name);
        if (it == parent_item.children_counts.end()) {
          return false;
        }
        int nth = it->value - 1;  // -1, because we increment the counter at
                                  // their start tags.
        if (nth != c.nth().index()) {
          return false;
        }
        break;
      }
      case ElementLocator_Component::COMPONENT_NOT_SET:
        NOTREACHED() << "ElementLocator_Component::component not populated";
        return false;
    }
  }
  return true;
}

}  // namespace

void TokenStreamMatcher::ObserveEndTag(const AtomicString& tag_name) {
  CHECK(!html_stack_.empty());

  wtf_size_t i;
  for (i = html_stack_.size() - 1; i > 0; --i) {
    if (html_stack_[i].tag_name == tag_name) {
      break;
    }
  }

  // Do not pop the sentinel root node.
  if (i == 0) {
    return;
  }

  html_stack_.Shrink(i);
}

#ifndef NDEBUG

void TokenStreamMatcher::DumpHTMLStack() {
  StringBuilder dump;
  for (const HTMLStackItem& item : html_stack_) {
    dump.Append("/");
    dump.Append(item.tag_name);
    if (!item.id_attr.empty()) {
      dump.Append("#");
      dump.Append(item.id_attr);
    }
    dump.Append("{");
    for (const auto& children_count : item.children_counts) {
      dump.Append(children_count.key);
      dump.Append('=');
      dump.AppendNumber(children_count.value);
      dump.Append(" ");
    }
    dump.Append("}");
  }

  LOG(ERROR) << "TokenStreamMatcher::html_stack_: "
             << dump.ReleaseString().Utf8();
}

#endif

bool TokenStreamMatcher::ObserveStartTagAndReportMatch(
    const AtomicString& tag_name,
    const AtomicString& id_attr) {
  // We implement a subset of
  // https://html.spec.whatwg.org/multipage/parsing.html#parsing-main-inbody

  // <spec>If the stack of open elements has a p element in button scope,
  // then close a p element.</spec>
  if (ClosePElementSet().Contains(tag_name)) {
    ObserveEndTag(html_names::kPTag.LocalName());
  }

  html_stack_.back().IncrementChildrenCount(tag_name);
  html_stack_.push_back(
      HTMLStackItem{.tag_name = tag_name, .id_attr = id_attr});

  auto stack_span = base::make_span(html_stack_.begin(), html_stack_.end());
  bool matched = false;
  for (const ElementLocator& locator : locators_) {
    if (MatchLocator(locator, stack_span)) {
      matched = true;
      break;
    }
  }

  if (ImmediatelyPopTheCurrentNodeTags().Contains(tag_name)) {
    ObserveEndTag(tag_name);
  }

  return matched;
}

}  // namespace blink::element_locator
