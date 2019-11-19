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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_VTT_VTT_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_VTT_VTT_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

enum VTTNodeType {
  kVTTNodeTypeNone = 0,
  kVTTNodeTypeClass,
  kVTTNodeTypeItalic,
  kVTTNodeTypeLanguage,
  kVTTNodeTypeBold,
  kVTTNodeTypeUnderline,
  kVTTNodeTypeRuby,
  kVTTNodeTypeRubyText,
  kVTTNodeTypeVoice
};

class VTTElement final : public Element {
 public:
  HTMLElement* CreateEquivalentHTMLElement(Document&);

  VTTElement(const QualifiedName&, Document*);
  VTTElement(VTTNodeType, Document*);

  Element& CloneWithoutAttributesAndChildren(Document&) const override;

  void SetVTTNodeType(VTTNodeType type) {
    web_vtt_node_type_ = static_cast<unsigned>(type);
  }
  VTTNodeType WebVTTNodeType() const {
    return static_cast<VTTNodeType>(web_vtt_node_type_);
  }

  bool IsPastNode() const { return is_past_node_; }
  void SetIsPastNode(bool);

  bool IsVTTElement() const override { return true; }
  AtomicString Language() const { return language_; }
  void SetLanguage(AtomicString value) { language_ = value; }

  static const QualifiedName& VoiceAttributeName() {
    DEFINE_STATIC_LOCAL(QualifiedName, voice_attr,
                        (g_null_atom, "voice", g_null_atom));
    return voice_attr;
  }

  static const QualifiedName& LangAttributeName() {
    DEFINE_STATIC_LOCAL(QualifiedName, voice_attr,
                        (g_null_atom, "lang", g_null_atom));
    return voice_attr;
  }

  const TextTrack* GetTrack() const { return track_; }

  void SetTrack(TextTrack*);
  void Trace(blink::Visitor*) override;

 private:
  Member<TextTrack> track_;
  unsigned is_past_node_ : 1;
  unsigned web_vtt_node_type_ : 4;

  AtomicString language_;
};

template <>
struct DowncastTraits<VTTElement> {
  static bool AllowFrom(const Node& node) { return node.IsVTTElement(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_VTT_VTT_ELEMENT_H_
