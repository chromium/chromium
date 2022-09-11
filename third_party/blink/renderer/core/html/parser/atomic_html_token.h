/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_ATOMIC_HTML_TOKEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_ATOMIC_HTML_TOKEN_H_

#include <memory>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/element_data.h"
#include "third_party/blink/renderer/core/dom/element_data_cache.h"
#include "third_party/blink/renderer/core/html/parser/html_attribute_buffer.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/core/html_element_lookup_trie.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class AtomicHTMLToken;

// HTMLTokenName represents a parsed token name (the local name of a
// QualifiedName). The token name contains the local name as an AtomicString and
// if the name is a valid html tag name, an HTMLTag. As this class is created
// from tokenized input, it does not know the namespace, the namespace is
// determined later in parsing (see HTMLTreeBuilder and HTMLStackItem).
class CORE_EXPORT HTMLTokenName {
 public:
  explicit HTMLTokenName(html_names::HTMLTag tag) : tag_(tag) {
    if (tag != html_names::HTMLTag::kUnknown)
      local_name_ = html_names::TagToQualifedName(tag).LocalName();
  }

  // Returns an HTMLTokenName for the specified string. This function looks up
  // the HTMLTag from the supplied string.
  static HTMLTokenName FromLocalName(const AtomicString& local_name) {
    if (local_name.IsEmpty())
      return HTMLTokenName(html_names::HTMLTag::kUnknown);

    if (local_name.Is8Bit()) {
      return HTMLTokenName(
          lookupHTMLTag(local_name.Characters8(), local_name.length()),
          local_name);
    }
    return HTMLTokenName(
        lookupHTMLTag(local_name.Characters16(), local_name.length()),
        local_name);
  }

  bool operator==(const HTMLTokenName& other) const {
    return other.local_name_ == local_name_;
  }

  bool IsValidHTMLTag() const { return tag_ != html_names::HTMLTag::kUnknown; }

  html_names::HTMLTag GetHTMLTag() const { return tag_; }

  const AtomicString& GetLocalName() const { return local_name_; }

 private:
  // For access to constructor.
  friend class AtomicHTMLToken;

  explicit HTMLTokenName(html_names::HTMLTag tag, const AtomicString& name)
      : tag_(tag), local_name_(name) {
#if DCHECK_IS_ON()
    if (tag == html_names::HTMLTag::kUnknown) {
      // If the tag is unknown, then `name` must either be empty, or not
      // identify any other HTMLTag.
      if (!name.IsEmpty()) {
        if (name.Is8Bit()) {
          DCHECK_EQ(html_names::HTMLTag::kUnknown,
                    lookupHTMLTag(name.Characters8(), name.length()));
        } else {
          DCHECK_EQ(html_names::HTMLTag::kUnknown,
                    lookupHTMLTag(name.Characters16(), name.length()));
        }
      }
    }
#endif
  }

  // This constructor is intended for use by AtomicHTMLToken when it is known
  // the string is not a known html tag.
  explicit HTMLTokenName(const AtomicString& name)
      : HTMLTokenName(html_names::HTMLTag::kUnknown, name) {}

  // Store both the tag and the name. The tag is enough to lookup the name, but
  // enough code makes use of the name that's it's worth caching (this performs
  // a bit better than using a variant for the two and looking up on demand).
  html_names::HTMLTag tag_;
  AtomicString local_name_;
};

class CORE_EXPORT AtomicHTMLToken {
  STACK_ALLOCATED();

 public:
  bool ForceQuirks() const {
    DCHECK_EQ(type_, HTMLToken::DOCTYPE);
    return doctype_data_->force_quirks_;
  }

  HTMLToken::TokenType GetType() const { return type_; }

  // TODO(sky): for consistency, rename to GetLocalName().
  const AtomicString& GetName() const {
    DCHECK(UsesName());
    return name_.GetLocalName();
  }

  void SetTokenName(const HTMLTokenName& name) {
    DCHECK(UsesName());
    name_ = name;
  }

  html_names::HTMLTag GetHTMLTag() const { return name_.GetHTMLTag(); }

  bool IsValidHTMLTag() const { return name_.IsValidHTMLTag(); }

  const HTMLTokenName& GetTokenName() const { return name_; }

  bool SelfClosing() const {
    DCHECK(type_ == HTMLToken::kStartTag || type_ == HTMLToken::kEndTag);
    return self_closing_;
  }

  bool HasDuplicateAttribute() const {
    return element_data_ && element_data_->has_duplicate_attribute_;
  }

  const Attribute* GetAttributeItem(const QualifiedName& attribute_name) {
    DCHECK(UsesAttributes());
    return element_data_ ? element_data_->Attributes().Find(attribute_name)
                         : nullptr;
  }

  void SetElementData(ShareableElementData* data) {
    DCHECK(UsesAttributes());
    element_data_ = data;
  }

  ShareableElementData* GetElementData() {
    DCHECK(UsesAttributes());
    return element_data_;
  }

  const String& Characters() const {
    DCHECK_EQ(type_, HTMLToken::kCharacter);
    return data_;
  }

  const String& Comment() const {
    DCHECK_EQ(type_, HTMLToken::kComment);
    return data_;
  }

  // FIXME: Distinguish between a missing public identifer and an empty one.
  Vector<UChar>& PublicIdentifier() const {
    DCHECK_EQ(type_, HTMLToken::DOCTYPE);
    return doctype_data_->public_identifier_;
  }

  // FIXME: Distinguish between a missing system identifer and an empty one.
  Vector<UChar>& SystemIdentifier() const {
    DCHECK_EQ(type_, HTMLToken::DOCTYPE);
    return doctype_data_->system_identifier_;
  }

  AtomicHTMLToken(HTMLToken& token, ElementDataCache* cache)
      : type_(token.GetType()), name_(HTMLTokenNameFromToken(token)) {
    switch (type_) {
      case HTMLToken::kUninitialized:
        NOTREACHED();
        break;
      case HTMLToken::DOCTYPE:
        doctype_data_ = token.ReleaseDoctypeData();
        break;
      case HTMLToken::kEndOfFile:
        break;
      case HTMLToken::kStartTag:
      case HTMLToken::kEndTag: {
        self_closing_ = token.SelfClosing();
        const auto& attribute_buffer = token.AttributeBuffer();
        const wtf_size_t num_attributes = attribute_buffer.NumberOfAttributes();

        if (!num_attributes)
          return;

        absl::optional<AtomicString> all_attributes_and_values_string;
        if (cache) {
          all_attributes_and_values_string =
              attribute_buffer.StringWithAllAttributesAndValues();
          element_data_ =
              cache->GetCachedDataByString(*all_attributes_and_values_string);
          if (element_data_)
            return;
        }

        // This limit is set fairly arbitrarily; the main point is to avoid
        // DDoS opportunities or similar with O(n²) behavior by setting lots
        // of attributes.
        const int kMinimumNumAttributesToDedupWithHash = 10;
        if (num_attributes >= kMinimumNumAttributesToDedupWithHash) {
          CreateElementDataFromAttributeBuffer</*DedupWithHash=*/true>(
              attribute_buffer, num_attributes);
        } else if (num_attributes) {
          CreateElementDataFromAttributeBuffer</*DedupWithHash=*/false>(
              attribute_buffer, num_attributes);
        }
        if (cache) {
          cache->AddDataForString(*all_attributes_and_values_string,
                                  *element_data_);
        }
        break;
      }
      case HTMLToken::kCharacter:
      case HTMLToken::kComment:
        if (token.IsAll8BitData())
          data_ = token.Data().AsString8();
        else
          data_ = token.Data().AsString();
        break;
    }
  }

  AtomicHTMLToken(HTMLToken::TokenType type,
                  html_names::HTMLTag tag,
                  ShareableElementData* element_data = nullptr)
      : type_(type), name_(tag), element_data_(element_data) {
    DCHECK(UsesName());
  }

  AtomicHTMLToken(HTMLToken::TokenType type,
                  const HTMLTokenName& name,
                  ShareableElementData* element_data = nullptr)
      : type_(type), name_(name), element_data_(element_data) {
    DCHECK(UsesName());
  }

  AtomicHTMLToken(const AtomicHTMLToken&) = delete;
  AtomicHTMLToken& operator=(const AtomicHTMLToken&) = delete;

#ifndef NDEBUG
  void Show() const;
#endif

 private:
  static HTMLTokenName HTMLTokenNameFromToken(const HTMLToken& token) {
    switch (token.GetType()) {
      case HTMLToken::DOCTYPE:
        // Doctype name may be empty, but not start/end tags.
        if (token.GetName().IsEmpty())
          return HTMLTokenName(html_names::HTMLTag::kUnknown);
        [[fallthrough]];
      case HTMLToken::kStartTag:
      case HTMLToken::kEndTag: {
        const html_names::HTMLTag html_tag =
            lookupHTMLTag(token.GetName().data(), token.GetName().size());
        if (html_tag != html_names::HTMLTag::kUnknown)
          return HTMLTokenName(html_tag);
        return HTMLTokenName(token.GetName().AsAtomicString());
      }
      default:
        return HTMLTokenName(html_names::HTMLTag::kUnknown);
    }
  }

  // Creates and sets `element_data_`. If `attribute_buffer` contains duplicate
  // attributes only the first value is kept.
  //
  // We can deduplicate attributes in two ways; using a hash table
  // (DedupWithHash=true) or by simple linear scanning (DedupWithHash=false).
  // If we don't have many attributes, the linear scan is cheaper than
  // setting up and searching in a hash table, even though the big-O
  // complexity is higher. Thus, we use the hash table only if the caller
  // expects a lot of attributes.
  template <bool DedupWithHash>
  ALWAYS_INLINE void CreateElementDataFromAttributeBuffer(
      const HTMLAttributeBuffer& attribute_buffer,
      wtf_size_t num_attributes);

  bool UsesName() const;

  bool UsesAttributes() const;

  HTMLToken::TokenType type_;

  // "name" for DOCTYPE, StartTag, and EndTag
  HTMLTokenName name_;

  // "data" for Comment, "characters" for Character
  String data_;

  // For DOCTYPE
  std::unique_ptr<DoctypeData> doctype_data_;

  // For StartTag and EndTag
  bool self_closing_ = false;

  ShareableElementData* element_data_ = nullptr;
};

template <bool DedupWithHash>
void AtomicHTMLToken::CreateElementDataFromAttributeBuffer(
    const HTMLAttributeBuffer& attribute_buffer,
    wtf_size_t num_attributes) {
  // Track which attributes have already been inserted to avoid N^2
  // behavior with repeated linear searches when populating `attributes_`.
  std::conditional_t<DedupWithHash, HashSet<AtomicString>, int>
      added_attributes;
  if constexpr (DedupWithHash) {
    added_attributes.ReserveCapacityForSize(num_attributes);
  }

  // This is only called once, so `element_data_` should be empty.
  DCHECK(!element_data_);
  Vector<Attribute, kAttributePrealloc> attributes;
  attributes.ReserveInitialCapacity(num_attributes);
  bool duplicate_attribute = false;
  for (HTMLAttributeBufferIterator iter(attribute_buffer); !iter.AtEnd();
       iter.Next()) {
    if (iter.IsNameEmpty())
      continue;

    QualifiedName name = iter.NameAsQualifiedName();
    if constexpr (DedupWithHash) {
      if (!added_attributes.insert(name.LocalName()).is_new_entry) {
        duplicate_attribute = true;
        continue;
      }
    } else {
      if (base::Contains(attributes, name.LocalName(), &Attribute::LocalName)) {
        duplicate_attribute = true;
        continue;
      }
    }

    attributes.UncheckedAppend(
        Attribute(std::move(name), iter.ValueAsAtomicString()));
  }
  element_data_ = ShareableElementData::CreateWithAttributes(
      attributes, duplicate_attribute);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_ATOMIC_HTML_TOKEN_H_
