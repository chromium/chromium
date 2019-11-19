/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/html/track/vtt/vtt_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

static const QualifiedName& NodeTypeToTagName(VTTNodeType node_type) {
  DEFINE_STATIC_LOCAL(QualifiedName, c_tag, (g_null_atom, "c", g_null_atom));
  DEFINE_STATIC_LOCAL(QualifiedName, v_tag, (g_null_atom, "v", g_null_atom));
  DEFINE_STATIC_LOCAL(QualifiedName, lang_tag,
                      (g_null_atom, "lang", g_null_atom));
  DEFINE_STATIC_LOCAL(QualifiedName, b_tag, (g_null_atom, "b", g_null_atom));
  DEFINE_STATIC_LOCAL(QualifiedName, u_tag, (g_null_atom, "u", g_null_atom));
  DEFINE_STATIC_LOCAL(QualifiedName, i_tag, (g_null_atom, "i", g_null_atom));
  DEFINE_STATIC_LOCAL(QualifiedName, ruby_tag,
                      (g_null_atom, "ruby", g_null_atom));
  DEFINE_STATIC_LOCAL(QualifiedName, rt_tag, (g_null_atom, "rt", g_null_atom));
  switch (node_type) {
    case kVTTNodeTypeClass:
      return c_tag;
    case kVTTNodeTypeItalic:
      return i_tag;
    case kVTTNodeTypeLanguage:
      return lang_tag;
    case kVTTNodeTypeBold:
      return b_tag;
    case kVTTNodeTypeUnderline:
      return u_tag;
    case kVTTNodeTypeRuby:
      return ruby_tag;
    case kVTTNodeTypeRubyText:
      return rt_tag;
    case kVTTNodeTypeVoice:
      return v_tag;
    case kVTTNodeTypeNone:
    default:
      NOTREACHED();
      return c_tag;  // Make the compiler happy.
  }
}

VTTElement::VTTElement(VTTNodeType node_type, Document* document)
    : Element(NodeTypeToTagName(node_type), document, kCreateElement),
      is_past_node_(0),
      web_vtt_node_type_(node_type) {}

Element& VTTElement::CloneWithoutAttributesAndChildren(
    Document& factory) const {
  auto* clone = MakeGarbageCollected<VTTElement>(
      static_cast<VTTNodeType>(web_vtt_node_type_), &factory);
  clone->SetLanguage(language_);
  clone->SetTrack(track_);
  return *clone;
}

HTMLElement* VTTElement::CreateEquivalentHTMLElement(Document& document) {
  Element* html_element = nullptr;
  switch (web_vtt_node_type_) {
    case kVTTNodeTypeClass:
    case kVTTNodeTypeLanguage:
    case kVTTNodeTypeVoice:
      html_element = document.CreateRawElement(html_names::kSpanTag,
                                               CreateElementFlags::ByParser());
      html_element->setAttribute(html_names::kTitleAttr,
                                 getAttribute(VoiceAttributeName()));
      html_element->setAttribute(html_names::kLangAttr,
                                 getAttribute(LangAttributeName()));
      break;
    case kVTTNodeTypeItalic:
      html_element = document.CreateRawElement(html_names::kITag,
                                               CreateElementFlags::ByParser());
      break;
    case kVTTNodeTypeBold:
      html_element = document.CreateRawElement(html_names::kBTag,
                                               CreateElementFlags::ByParser());
      break;
    case kVTTNodeTypeUnderline:
      html_element = document.CreateRawElement(html_names::kUTag,
                                               CreateElementFlags::ByParser());
      break;
    case kVTTNodeTypeRuby:
      html_element = document.CreateRawElement(html_names::kRubyTag,
                                               CreateElementFlags::ByParser());
      break;
    case kVTTNodeTypeRubyText:
      html_element = document.CreateRawElement(html_names::kRtTag,
                                               CreateElementFlags::ByParser());
      break;
    default:
      NOTREACHED();
  }

  html_element->setAttribute(html_names::kClassAttr,
                             getAttribute(html_names::kClassAttr));
  return To<HTMLElement>(html_element);
}

void VTTElement::SetIsPastNode(bool is_past_node) {
  if (!!is_past_node_ == is_past_node)
    return;

  is_past_node_ = is_past_node;
  SetNeedsStyleRecalc(
      kLocalStyleChange,
      StyleChangeReasonForTracing::CreateWithExtraData(
          style_change_reason::kPseudoClass, style_change_extra_data::g_past));
}

void VTTElement::SetTrack(TextTrack* track) {
  track_ = track;
}

void VTTElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(track_);
  Element::Trace(visitor);
}

}  // namespace blink
