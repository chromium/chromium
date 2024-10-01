// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/lcp_critical_path_predictor/element_locator.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::element_locator {

ElementLocator OfElement(const Element& element) {
  ElementLocator locator;

  Element* element_ptr = const_cast<Element*>(&element);
  while (element_ptr) {
    Element* parent = element_ptr->parentElement();

    if (element_ptr->HasID()) {
      // Peg on element id if that exists

      ElementLocator_Component_Id* id_comp =
          locator.add_components()->mutable_id();
      id_comp->set_id_attr(element_ptr->GetIdAttribute().Utf8());
      break;
    } else if (parent) {
      // Last resort: n-th element that has the `tag_name`.

      AtomicString tag_name = element_ptr->localName();

      int nth = 0;
      for (Node* sibling = parent->firstChild(); sibling;
           sibling = sibling->nextSibling()) {
        Element* sibling_el = DynamicTo<Element>(sibling);
        if (!sibling_el || sibling_el->localName() != tag_name) {
          continue;
        }

        if (sibling_el == element_ptr) {
          ElementLocator_Component_NthTagName* nth_comp =
              locator.add_components()->mutable_nth();
          nth_comp->set_tag_name(tag_name.Utf8());
          nth_comp->set_index(nth);
          break;
        }

        ++nth;
      }
    }

    element_ptr = parent;
  }

  return locator;
}

String ToStringForTesting(const ElementLocator& locator) {
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
    const StringImpl* children_tag_name) {
  auto add_result = children_counts.insert(children_tag_name, 1);
  if (!add_result.is_new_entry) {
    ++add_result.stored_value->value;
  }
}

namespace {

// Set of element tag names that needs to run a "close a p element" step in
// https://html.spec.whatwg.org/multipage/parsing.html#parsing-main-inbody
// Do not modify this set outside TokenStreamMatcher::InitSets() to avoid race
// conditions.
HashSet<const StringImpl*>& ClosePElementSet() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashSet<const StringImpl*>, set, ());
  return set;
}

// The list of tags that their start tag tokens need to be closed immediately,
// with the following spec text:
// <spec>Insert an HTML element for the token. Immediately pop the current node
// off the stack of open elements.</spec>
// Do not modify this set outside TokenStreamMatcher::InitSets() to avoid race
// conditions.
HashSet<const StringImpl*>& ImmediatelyPopTheCurrentNodeTags() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashSet<const StringImpl*>, set, ());
  return set;
}

// A restricted of tags against which this TokenStreamMatcher will initiate
// a match, when match_against_restricted_set flag is turned on, to reduce
// performance hit.
HashSet<const StringImpl*>& RestrictedTagSubset() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashSet<const StringImpl*>, set, ());
  return set;
}

}  // namespace

void TokenStreamMatcher::InitSets() {
  {
    HashSet<const StringImpl*>& set = ClosePElementSet();
    set.insert(html_names::kAddressTag.LocalName().Impl());
    set.insert(html_names::kArticleTag.LocalName().Impl());
    set.insert(html_names::kAsideTag.LocalName().Impl());
    set.insert(html_names::kBlockquoteTag.LocalName().Impl());
    set.insert(html_names::kCenterTag.LocalName().Impl());
    set.insert(html_names::kDetailsTag.LocalName().Impl());
    set.insert(html_names::kDirTag.LocalName().Impl());
    set.insert(html_names::kDivTag.LocalName().Impl());
    set.insert(html_names::kDlTag.LocalName().Impl());
    set.insert(html_names::kFieldsetTag.LocalName().Impl());
    set.insert(html_names::kFigcaptionTag.LocalName().Impl());
    set.insert(html_names::kFigureTag.LocalName().Impl());
    set.insert(html_names::kFooterTag.LocalName().Impl());
    set.insert(html_names::kHeaderTag.LocalName().Impl());
    set.insert(html_names::kHgroupTag.LocalName().Impl());
    set.insert(html_names::kMainTag.LocalName().Impl());
    set.insert(html_names::kMenuTag.LocalName().Impl());
    set.insert(html_names::kNavTag.LocalName().Impl());
    set.insert(html_names::kOlTag.LocalName().Impl());
    set.insert(html_names::kPTag.LocalName().Impl());

    // The spec says that we should run the step for the "search" tag as well,
    // but we don't have the implementation in Blink yet.
    // set.insert(html_names::kSearchTag.LocalName().Impl());

    set.insert(html_names::kSectionTag.LocalName().Impl());
    set.insert(html_names::kSummaryTag.LocalName().Impl());
    set.insert(html_names::kUlTag.LocalName().Impl());

    set.insert(html_names::kH1Tag.LocalName().Impl());
    set.insert(html_names::kH2Tag.LocalName().Impl());
    set.insert(html_names::kH3Tag.LocalName().Impl());
    set.insert(html_names::kH4Tag.LocalName().Impl());
    set.insert(html_names::kH5Tag.LocalName().Impl());
    set.insert(html_names::kH6Tag.LocalName().Impl());

    set.insert(html_names::kPreTag.LocalName().Impl());
    set.insert(html_names::kListingTag.LocalName().Impl());
    set.insert(html_names::kFormTag.LocalName().Impl());
    set.insert(html_names::kPlaintextTag.LocalName().Impl());

    set.insert(html_names::kXmpTag.LocalName().Impl());
  }
  {
    HashSet<const StringImpl*>& set = ImmediatelyPopTheCurrentNodeTags();
    set.insert(html_names::kAreaTag.LocalName().Impl());
    set.insert(html_names::kBrTag.LocalName().Impl());
    set.insert(html_names::kEmbedTag.LocalName().Impl());
    set.insert(html_names::kImgTag.LocalName().Impl());
    set.insert(html_names::kKeygenTag.LocalName().Impl());
    set.insert(html_names::kWbrTag.LocalName().Impl());
    set.insert(html_names::kInputTag.LocalName().Impl());
    set.insert(html_names::kParamTag.LocalName().Impl());
    set.insert(html_names::kSourceTag.LocalName().Impl());
    set.insert(html_names::kTrackTag.LocalName().Impl());
    set.insert(html_names::kHrTag.LocalName().Impl());
  }
  {
    HashSet<const StringImpl*>& set = RestrictedTagSubset();
    set.insert(html_names::kImgTag.LocalName().Impl());
  }
}

TokenStreamMatcher::TokenStreamMatcher(Vector<ElementLocator> locators)
    : locators_(locators) {}

TokenStreamMatcher::~TokenStreamMatcher() = default;
namespace {

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
        const std::string& tag_name_stdstr = c.nth().tag_name();
        AtomicString tag_name(base::as_byte_span(tag_name_stdstr));
        if (!tag_name.Impl()->IsStatic()) {
          // `tag_name` should only contain one of the known HTML tags.
          return false;
        }

        // Check if tag_name matches
        if (matched_item.tag_name != tag_name.Impl()) {
          return false;
        }

        // Check if the element is actually the nth
        // child of its parent.
        auto it = parent_item.children_counts.find(matched_item.tag_name);
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
        NOTREACHED_IN_MIGRATION()
            << "ElementLocator_Component::component not populated";
        return false;
    }
  }
  return true;
}

}  // namespace

void TokenStreamMatcher::ObserveEndTag(const StringImpl* tag_name) {
  CHECK(!html_stack_.empty());

  // Don't build stack if locators are empty.
  if (locators_.empty()) {
    return;
  }

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
    const StringImpl* tag_name,
    const HTMLToken& token) {
  // If `tag_name` is null, ignore.
  // "Custom Elements" will hit this condition.
  if (!tag_name) {
    return false;
  }

  // Don't build stack if locators are empty.
  if (locators_.empty()) {
    return false;
  }

  // We implement a subset of
  // https://html.spec.whatwg.org/multipage/parsing.html#parsing-main-inbody

  // <spec>If the stack of open elements has a p element in button scope,
  // then close a p element.</spec>
  DCHECK(!ClosePElementSet().empty());
  if (ClosePElementSet().Contains(tag_name)) {
    ObserveEndTag(html_names::kPTag.LocalName().Impl());
  }

  html_stack_.back().IncrementChildrenCount(tag_name);
  const HTMLToken::Attribute* id_attr =
      token.GetAttributeItem(html_names::kIdAttr);
  html_stack_.push_back(HTMLStackItem{
      .tag_name = tag_name,
      .id_attr = id_attr ? AtomicString(id_attr->Value()) : g_null_atom});

  bool matched = false;
  // Invoke matching only if set to match all tags, or this is an IMG tag.
  if (RestrictedTagSubset().Contains(tag_name)) {
    auto stack_span = base::make_span(html_stack_.begin(), html_stack_.end());
    for (const ElementLocator& locator : locators_) {
      if (MatchLocator(locator, stack_span)) {
        matched = true;
        break;
      }
    }
  }

  DCHECK(!ImmediatelyPopTheCurrentNodeTags().empty());
  if (ImmediatelyPopTheCurrentNodeTags().Contains(tag_name)) {
    ObserveEndTag(tag_name);
  }

  return matched;
}

}  // namespace blink::element_locator
