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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_TOKEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_TOKEN_H_

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/html/parser/html_attribute_buffer.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/literal_buffer.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class DoctypeData {
  USING_FAST_MALLOC(DoctypeData);

 public:
  DoctypeData()
      : has_public_identifier_(false),
        has_system_identifier_(false),
        force_quirks_(false) {}
  DoctypeData(const DoctypeData&) = delete;
  DoctypeData& operator=(const DoctypeData&) = delete;

  bool has_public_identifier_;
  bool has_system_identifier_;
  WTF::Vector<UChar> public_identifier_;
  WTF::Vector<UChar> system_identifier_;
  bool force_quirks_;
};

static inline Attribute* FindAttributeInVector(base::span<Attribute> attributes,
                                               const QualifiedName& name) {
  for (Attribute& attr : attributes) {
    if (attr.GetName().Matches(name))
      return &attr;
  }
  return nullptr;
}

class HTMLToken {
  USING_FAST_MALLOC(HTMLToken);

 public:
  enum TokenType {
    kUninitialized,
    DOCTYPE,
    kStartTag,
    kEndTag,
    kComment,
    kCharacter,
    kEndOfFile,
  };

  // TODO(https://crbug.com/1361410): remove this.
  class Attribute {
    DISALLOW_NEW();

   public:
    class Range {
      DISALLOW_NEW();

     public:
      static constexpr int kInvalidOffset = -1;

      inline void Clear() {
#if DCHECK_IS_ON()
        start = kInvalidOffset;
        end = kInvalidOffset;
#endif
      }

      // Check Range instance that is actively being parsed.
      inline void CheckValidStart() const {
        DCHECK_NE(start, kInvalidOffset);
        DCHECK_GE(start, 0);
      }

      // Check Range instance which finished parse.
      inline void CheckValid() const {
        CheckValidStart();
        DCHECK_NE(end, kInvalidOffset);
        DCHECK_GE(end, 0);
        DCHECK_LE(start, end);
      }

      int start;
      int end;
    };

    AtomicString GetName() const { return name_.AsAtomicString(); }
    AtomicString GetValue() const { return value_.AsAtomicString(); }

    const UCharLiteralBuffer<32>& NameBuffer() const { return name_; }
    UCharLiteralBuffer<32>& NameBuffer() { return name_; }
    UCharLiteralBuffer<32>& ValueBuffer() { return value_; }

    String NameAttemptStaticStringCreation() const {
      return AttemptStaticStringCreation(name_, kLikely8Bit);
    }

    bool NameIsEmpty() const { return name_.IsEmpty(); }
    void AppendToName(UChar c) { name_.AddChar(c); }

    String Value8BitIfNecessary() const {
      // TODO(https://crbug.com/1331076): remove this function and convert
      // callers to Value() once
      // `g_literal_buffer_create_string_with_encoding` is removed.
      if (!g_literal_buffer_create_string_with_encoding)
        return StringImpl::Create8BitIfPossible(value_.data(), value_.size());
      return value_.AsString();
    }
    String Value() const { return value_.AsString(); }

    void AppendToValue(UChar c) { value_.AddChar(c); }
    void ClearValue() { value_.clear(); }

    const Range& NameRange() const { return name_range_; }
    const Range& ValueRange() const { return value_range_; }
    Range& MutableNameRange() { return name_range_; }
    Range& MutableValueRange() { return value_range_; }

   private:
    // TODO(chromium:1204030): Do a more rigorous study and select a
    // better-informed inline capacity.
    UCharLiteralBuffer<32> name_;
    UCharLiteralBuffer<32> value_;
    Range name_range_;
    Range value_range_;
  };

  typedef Vector<Attribute, kAttributePrealloc> AttributeList;

  // By using an inline capacity of 256, we avoid spilling over into an malloced
  // buffer approximately 99% of the time based on a non-scientific browse
  // around a number of popular web sites on 23 May 2013.
  // TODO(chromium:1204030): Do a more rigorous study and select a
  // better-informed inline capacity.
  using DataVector = UCharLiteralBuffer<256>;

  HTMLToken() {
    range_.Clear();
    range_.start = 0;
  }

  HTMLToken(const HTMLToken&) = delete;
  HTMLToken& operator=(const HTMLToken&) = delete;

  void Clear() {
    if (type_ == kUninitialized)
      return;

    type_ = kUninitialized;
    range_.Clear();
    range_.start = 0;
    attribute_buffer_.Clear();
    base_offset_ = 0;
    data_.clear();
  }

  bool IsUninitialized() { return type_ == kUninitialized; }
  TokenType GetType() const { return type_; }

  void MakeEndOfFile() {
    DCHECK_EQ(type_, kUninitialized);
    type_ = kEndOfFile;
  }

  // Range and offset methods exposed for HTMLSourceTracker and
  // HTMLViewSourceParser.
  int StartIndex() const { return range_.start; }
  int EndIndex() const { return range_.end; }

  void SetBaseOffset(int offset) { base_offset_ = offset; }

  void end(int end_offset) { range_.end = end_offset - base_offset_; }

  const DataVector& Data() const {
    DCHECK(type_ == kCharacter || type_ == kComment || type_ == kStartTag ||
           type_ == kEndTag);
    return data_;
  }

  ALWAYS_INLINE bool IsAll8BitData() const { return data_.Is8Bit(); }

  const DataVector& GetName() const {
    DCHECK(type_ == kStartTag || type_ == kEndTag || type_ == DOCTYPE);
    return data_;
  }

  void AppendToName(UChar character) {
    DCHECK(type_ == kStartTag || type_ == kEndTag || type_ == DOCTYPE);
    DCHECK(character);
    data_.AddChar(character);
  }

  /* DOCTYPE Tokens */

  bool ForceQuirks() const {
    DCHECK_EQ(type_, DOCTYPE);
    return doctype_data_->force_quirks_;
  }

  void SetForceQuirks() {
    DCHECK_EQ(type_, DOCTYPE);
    doctype_data_->force_quirks_ = true;
  }

  void BeginDOCTYPE() {
    DCHECK_EQ(type_, kUninitialized);
    type_ = DOCTYPE;
    doctype_data_ = std::make_unique<DoctypeData>();
  }

  void BeginDOCTYPE(UChar character) {
    DCHECK(character);
    BeginDOCTYPE();
    data_.AddChar(character);
  }

  // FIXME: Distinguish between a missing public identifer and an empty one.
  const WTF::Vector<UChar>& PublicIdentifier() const {
    DCHECK_EQ(type_, DOCTYPE);
    return doctype_data_->public_identifier_;
  }

  // FIXME: Distinguish between a missing system identifer and an empty one.
  const WTF::Vector<UChar>& SystemIdentifier() const {
    DCHECK_EQ(type_, DOCTYPE);
    return doctype_data_->system_identifier_;
  }

  void SetPublicIdentifierToEmptyString() {
    DCHECK_EQ(type_, DOCTYPE);
    doctype_data_->has_public_identifier_ = true;
    doctype_data_->public_identifier_.clear();
  }

  void SetSystemIdentifierToEmptyString() {
    DCHECK_EQ(type_, DOCTYPE);
    doctype_data_->has_system_identifier_ = true;
    doctype_data_->system_identifier_.clear();
  }

  void AppendToPublicIdentifier(UChar character) {
    DCHECK(character);
    DCHECK_EQ(type_, DOCTYPE);
    DCHECK(doctype_data_->has_public_identifier_);
    doctype_data_->public_identifier_.push_back(character);
  }

  void AppendToSystemIdentifier(UChar character) {
    DCHECK(character);
    DCHECK_EQ(type_, DOCTYPE);
    DCHECK(doctype_data_->has_system_identifier_);
    doctype_data_->system_identifier_.push_back(character);
  }

  std::unique_ptr<DoctypeData> ReleaseDoctypeData() {
    return std::move(doctype_data_);
  }

  /* Start/End Tag Tokens */

  bool SelfClosing() const {
    DCHECK(type_ == kStartTag || type_ == kEndTag);
    return self_closing_;
  }

  void SetSelfClosing() {
    DCHECK(type_ == kStartTag || type_ == kEndTag);
    self_closing_ = true;
  }

  void BeginStartTag(UChar character) {
    DCHECK(character);
    DCHECK_EQ(type_, kUninitialized);
    type_ = kStartTag;
    self_closing_ = false;
    DCHECK(attribute_buffer_.IsEmpty());

    data_.AddChar(character);
  }

  void BeginEndTag(LChar character) {
    DCHECK_EQ(type_, kUninitialized);
    type_ = kEndTag;
    self_closing_ = false;
    DCHECK(attribute_buffer_.IsEmpty());

    data_.AddChar(character);
  }

  void BeginEndTag(const LCharLiteralBuffer<32>& characters) {
    DCHECK_EQ(type_, kUninitialized);
    type_ = kEndTag;
    self_closing_ = false;
    DCHECK(attribute_buffer_.IsEmpty());

    data_.AppendLiteral(characters);
  }

  void AddNewAttribute() {
    DCHECK(type_ == kStartTag || type_ == kEndTag);
    attribute_buffer_.AddNewAttribute();
  }

  void BeginAttributeName(int offset) {
    attribute_buffer_.BeginAttributeName(offset - base_offset_);
  }

  void EndAttributeName(int offset) {
    attribute_buffer_.EndAttributeName(offset - base_offset_);
  }

  void BeginAttributeValue(int offset) {
    attribute_buffer_.BeginAttributeValue(offset - base_offset_);
  }

  void EndAttributeValue(int offset) {
    attribute_buffer_.EndAttributeValue(offset - base_offset_);
  }

  void AppendToAttributeName(UChar character) {
    DCHECK(character);
    DCHECK(type_ == kStartTag || type_ == kEndTag);
    attribute_buffer_.AppendToAttributeName(character);
  }

  void AppendToAttributeValue(UChar character) {
    DCHECK(character);
    DCHECK(type_ == kStartTag || type_ == kEndTag);
    attribute_buffer_.AppendToAttributeValue(character);
  }

  const HTMLAttributeBuffer& AttributeBuffer() const {
    return attribute_buffer_;
  }

  // TODO(https://crbug.com/1361410): remove this.
  AttributeList CreateAttributeList() const;

  /* Character Tokens */

  // Starting a character token works slightly differently than starting
  // other types of tokens because we want to save a per-character branch.
  void EnsureIsCharacterToken() {
    DCHECK(type_ == kUninitialized || type_ == kCharacter);
    type_ = kCharacter;
  }

  const DataVector& Characters() const {
    DCHECK_EQ(type_, kCharacter);
    return data_;
  }

  void AppendToCharacter(char character) {
    DCHECK_EQ(type_, kCharacter);
    data_.AddChar(character);
  }

  void AppendToCharacter(UChar character) {
    DCHECK_EQ(type_, kCharacter);
    data_.AddChar(character);
  }

  void AppendToCharacter(const LCharLiteralBuffer<32>& characters) {
    DCHECK_EQ(type_, kCharacter);
    data_.AppendLiteral(characters);
  }

  /* Comment Tokens */

  const DataVector& Comment() const {
    DCHECK_EQ(type_, kComment);
    return data_;
  }

  void BeginComment() {
    DCHECK_EQ(type_, kUninitialized);
    type_ = kComment;
  }

  void AppendToComment(UChar character) {
    DCHECK(character);
    DCHECK_EQ(type_, kComment);
    data_.AddChar(character);
  }

 private:
  TokenType type_ = kUninitialized;
  Attribute::Range range_;  // Always starts at zero.
  int base_offset_ = 0;
  DataVector data_;

  // For StartTag and EndTag
  bool self_closing_;

  HTMLAttributeBuffer attribute_buffer_;

  // For DOCTYPE
  std::unique_ptr<DoctypeData> doctype_data_;
};

#ifndef NDEBUG
const char* ToString(HTMLToken::TokenType);
#endif

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_TOKEN_H_
