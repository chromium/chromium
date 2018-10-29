/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_DESCRIPTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_DESCRIPTOR_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

struct V0CustomElementDescriptorHash;

// A Custom Element descriptor is everything necessary to match a
// Custom Element instance to a definition.
class V0CustomElementDescriptor {
  DISALLOW_NEW();

 public:
  V0CustomElementDescriptor(const AtomicString& type,
                            const AtomicString& namespace_uri,
                            const AtomicString& local_name)
      : type_(type), namespace_uri_(namespace_uri), local_name_(local_name) {}

  V0CustomElementDescriptor() = default;
  ~V0CustomElementDescriptor() = default;

  // Specifies whether the custom element is in the HTML or SVG
  // namespace.
  const AtomicString& NamespaceURI() const { return namespace_uri_; }

  // The tag name.
  const AtomicString& LocalName() const { return local_name_; }

  // The name of the definition. For custom tags, this is the tag
  // name and the same as "localName". For type extensions, this is
  // the value of the "is" attribute.
  const AtomicString& GetType() const { return type_; }

  bool IsTypeExtension() const { return type_ != local_name_; }

  bool operator==(const V0CustomElementDescriptor& other) const {
    return type_ == other.type_ && local_name_ == other.local_name_ &&
           namespace_uri_ == other.namespace_uri_;
  }

 private:
  friend struct WTF::HashTraits<blink::V0CustomElementDescriptor>;
  AtomicString type_;
  AtomicString namespace_uri_;
  AtomicString local_name_;
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::V0CustomElementDescriptor> {
  typedef blink::V0CustomElementDescriptorHash Hash;
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_DESCRIPTOR_H_
