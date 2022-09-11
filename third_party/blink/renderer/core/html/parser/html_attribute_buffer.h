// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_ATTRIBUTE_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_ATTRIBUTE_BUFFER_H_

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/html/parser/literal_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

// TODO(https://crbug.com/1338583): enable on android.
#if !BUILDFLAG(IS_ANDROID)
#include "third_party/blink/renderer/core/html_element_attribute_name_lookup_trie.h"  // nogncheck
#endif

namespace blink {

// Controls whether attribute name lookup uses LookupHTMLAttributeName().
CORE_EXPORT extern bool g_use_html_attribute_name_lookup;

class HTMLAttributeBufferIterator;

// HTMLAttributeBuffer contains all the attribute names and values in a
// single LiteralBuffer. A single LiteralBuffer is used so that it's trivial
// to get a string representation for caching. The position of the attributes
// are stored separately. To iterate through the contents use
// HTMLAttributeBufferIterator. For example, for the HTML
// "<p id=foo class=bar>" the stored string will be "id\0foo\0class\0bar\0".
class HTMLAttributeBuffer {
  DISALLOW_NEW();

 public:
  // Separator between attribute names and values.
  static constexpr LChar kAttributeSeparator = '\0';

  bool IsEmpty() const { return ranges_.IsEmpty(); }

  wtf_size_t NumberOfAttributes() const { return ranges_.size(); }

  void Clear() {
    combined_attribute_buffer_.clear();
    ranges_.Shrink(0);
    current_range_ = nullptr;
  }

  void AddNewAttribute() {
    // Separate the names/values by null, which isn't valid for attribute
    // names/values.
    combined_attribute_buffer_.AddChar(kAttributeSeparator);
    current_range_ = &ranges_.emplace_back(combined_attribute_buffer_.size());
  }

  void AppendToAttributeBuffer(UChar c) {
    combined_attribute_buffer_.AddChar(c);
  }

  void BeginAttributeName(int offset) {
    DCHECK(current_range_);
    DCHECK_NE(offset, Range::kInvalidOffset);
    current_range_->name_start = offset;
  }

  void EndAttributeName(int offset) {
    DCHECK(current_range_);
    DCHECK_NE(offset, Range::kInvalidOffset);
    DCHECK_GE(offset, current_range_->value_start);
    current_range_->name_end = offset;
    current_range_->name_length_in_buffer =
        combined_attribute_buffer_.size() -
        current_range_->name_start_in_buffer;

    // Insert a separator to ensure attributes with no value are properly
    // differentiated. Without this the underlying string might look the same
    // even though name/value pair differs. For example, <p id=x> and <p id x>.
    combined_attribute_buffer_.AddChar(kAttributeSeparator);

    // It's possible BeginAttributeValue() won't be called, set the start/end
    // of the value now so that the range is valid.
    current_range_->value_start_in_buffer = combined_attribute_buffer_.size();
    current_range_->value_start = offset;
    current_range_->value_end = offset;
    current_range_->CheckValid();
  }

  void BeginAttributeValue(int offset) {
    DCHECK(current_range_);
    DCHECK_NE(offset, Range::kInvalidOffset);
    current_range_->value_start = offset;
  }

  void EndAttributeValue(int offset) {
    DCHECK(current_range_);
    DCHECK_NE(offset, Range::kInvalidOffset);
    DCHECK_GE(offset, current_range_->value_start);
    current_range_->value_end = offset;
    current_range_->value_length_in_buffer =
        combined_attribute_buffer_.size() -
        current_range_->value_start_in_buffer;
  }

  void AppendToAttributeName(UChar character) {
    DCHECK(current_range_);
    DCHECK_NE(Range::kInvalidOffset, current_range_->name_start);
    combined_attribute_buffer_.AddChar(character);
  }

  void AppendToAttributeValue(UChar character) {
    DCHECK(current_range_);
    DCHECK_NE(Range::kInvalidOffset, current_range_->value_start);
    combined_attribute_buffer_.AddChar(character);
  }

  // Returns a string containing all the attribute names and values. This is
  // intended for cache lookup and not useful outside of this use case.
  AtomicString StringWithAllAttributesAndValues() const {
    return combined_attribute_buffer_.AsAtomicString();
  }

 private:
  // Contains the ranges of the name and value of an attribute, as well as the
  // position in the buffer
  // (`HTMLAttributeBuffer::combined_attribute_buffer_`).
  class Range final {
    DISALLOW_NEW();

   public:
    static constexpr int kInvalidOffset = -1;

    explicit Range(wtf_size_t start_in_buffer)
        : name_start_in_buffer(start_in_buffer),
          name_length_in_buffer(0),
          value_length_in_buffer(0) {
#if DCHECK_IS_ON()
      DCHECK_NE(name_start_in_buffer, kNotFound);
      name_start = kInvalidOffset;
      name_end = kInvalidOffset;
      value_start = kInvalidOffset;
      value_end = kInvalidOffset;
      value_start_in_buffer = kNotFound;
#endif
    }

    // Asserts the range is valid.
    inline void CheckValid() const {
#if DCHECK_IS_ON()
      DCHECK_NE(name_start, kInvalidOffset);
      DCHECK_NE(name_end, kInvalidOffset);
      DCHECK_GE(name_start, 0);
      DCHECK_GE(name_end, name_start);
      DCHECK_NE(value_start, kInvalidOffset);
      DCHECK_NE(value_end, kInvalidOffset);
      DCHECK_GE(value_start, 0);
      DCHECK_GE(value_end, value_start);
      DCHECK_NE(value_start_in_buffer, kNotFound);
#endif
    }

    // NOTE: the length in the buffer is not necessarily the same as
    // (end - start). For escaped sequences the two will differ.
    int name_start;
    int name_end;
    wtf_size_t name_start_in_buffer;
    wtf_size_t name_length_in_buffer;

    int value_start;
    int value_end;
    wtf_size_t value_start_in_buffer;
    wtf_size_t value_length_in_buffer;
  };

  using Ranges = Vector<Range, kAttributePrealloc>;

  friend class HTMLAttributeBufferIterator;

  Ranges ranges_;
  Range* current_range_ = nullptr;
  UCharLiteralBuffer<256> combined_attribute_buffer_;
};

// Represents the offset into a document for an attribute.
struct HTMLAttributeRange {
  int start;
  int end;
};

// Used to iterate over the attributes in an HTMLAttributeBuffer.
class HTMLAttributeBufferIterator final {
 public:
  explicit HTMLAttributeBufferIterator(const HTMLAttributeBuffer& attributes)
      : current_range_(attributes.ranges_.data()),
        end_of_ranges_(current_range_ + attributes.ranges_.size()),
        buffer_start_(attributes.combined_attribute_buffer_.data()) {
#if DCHECK_IS_ON()
    for (const auto& range : attributes.ranges_)
      range.CheckValid();
#endif
  }

  // Returns the current name as a QualifiedName. As QualifiedName isn't
  // thread safe, this should only be used when running on the main thread.
  // Code running on another thread should use name().
  QualifiedName NameAsQualifiedName() const {
    DCHECK(!AtEnd());
    QualifiedName name = g_null_name;
#if !BUILDFLAG(IS_ANDROID)
    if (g_use_html_attribute_name_lookup) {
      name = LookupHTMLAttributeName(
          buffer_start_ + current_range_->name_start_in_buffer,
          current_range_->name_length_in_buffer);
    }
#endif
    if (name == g_null_name) {
      name = QualifiedName(
          g_null_atom,
          AtomicString(buffer_start_ + current_range_->name_start_in_buffer,
                       current_range_->name_length_in_buffer),
          g_null_atom);
    }
    return name;
  }

  bool IsNameEmpty() const {
    DCHECK(!AtEnd());
    return !current_range_->name_length_in_buffer;
  }

  StringView name() const {
    DCHECK(!AtEnd());
    return StringView(buffer_start_ + current_range_->name_start_in_buffer,
                      current_range_->name_length_in_buffer);
  }

  HTMLAttributeRange name_range() {
    DCHECK(!AtEnd());
    return HTMLAttributeRange{.start = current_range_->name_start,
                              .end = current_range_->name_end};
  }

  StringView value() const {
    DCHECK(!AtEnd());
    return StringView(buffer_start_ + current_range_->value_start_in_buffer,
                      current_range_->value_length_in_buffer);
  }

  HTMLAttributeRange value_range() {
    DCHECK(!AtEnd());
    return HTMLAttributeRange{.start = current_range_->value_start,
                              .end = current_range_->value_end};
  }

  AtomicString ValueAsAtomicString() const {
    DCHECK(!AtEnd());
    return current_range_->value_length_in_buffer == 0
               ? g_empty_atom
               : AtomicString(
                     buffer_start_ + current_range_->value_start_in_buffer,
                     current_range_->value_length_in_buffer);
  }

  bool AtEnd() const { return current_range_ == end_of_ranges_; }

  void Next() {
    if (AtEnd())
      return;
    ++current_range_;
  }

 private:
  const HTMLAttributeBuffer::Range* current_range_;
  const HTMLAttributeBuffer::Range* end_of_ranges_;
  const UChar* buffer_start_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_ATTRIBUTE_BUFFER_H_
