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

#include "base/macros.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class DoctypeData {
  USING_FAST_MALLOC(DoctypeData);

 public:
  DoctypeData()
      : has_public_identifier_(false),
        has_system_identifier_(false),
        force_quirks_(false) {}

  bool has_public_identifier_;
  bool has_system_identifier_;
  WTF::Vector<UChar> public_identifier_;
  WTF::Vector<UChar> system_identifier_;
  bool force_quirks_;

  DISALLOW_COPY_AND_ASSIGN(DoctypeData);
};

static inline Attribute* FindAttributeInVector(Vector<Attribute>& attributes,
                                               const QualifiedName& name) {
  for (unsigned i = 0; i < attributes.size(); ++i) {
    if (attributes.at(i).GetName().Matches(name))
      return &attributes.at(i);
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

    AtomicString GetName() const { return AtomicString(name_); }
    String NameAttemptStaticStringCreation() const {
      return AttemptStaticStringCreation(name_, kLikely8Bit);
    }
    const Vector<UChar, 32>& NameAsVector() const { return name_; }
    const Vector<UChar, 32>& ValueAsVector() const { return value_; }

    void AppendToName(UChar c) { name_.push_back(c); }

    scoped_refptr<StringImpl> Value8BitIfNecessary() const {
      return StringImpl::Create8BitIfPossible(value_);
    }
    String Value() const { return String(value_); }

    void AppendToValue(UChar c) { value_.push_back(c); }
    void AppendToValue(const String& value) { value.AppendTo(value_); }
    void ClearValue() { value_.clear(); }

    const Range& NameRange() const { return name_range_; }
    const Range& ValueRange() const { return value_range_; }
    Range& MutableNameRange() { return name_range_; }
    Range& MutableValueRange() { return value_range_; }

   private:
    Vector<UChar, 32> name_;
    Vector<UChar, 32> value_;
    Range name_range_;
    Range value_range_;
  };

  typedef Vector<Attribute, 10> AttributeList;

  // By using an inline capacity of 256, we avoid spilling over into an malloced
  // buffer approximately 99% of the time based on a non-scientific browse
  // around a number of popular web sites on 23 May 2013.
  typedef Vector<UChar, 256> DataVector;

  HTMLToken() { Clear(); }

  void Clear() {
    type_ = kUninitialized;
    range_.Clear();
    range_.start = 0;
    base_offset_ = 0;
    // Don't call Vector::clear() as that would destroy the
    // alloced VectorBuffer. If the innerHTML'd content has
    // two 257 character text nodes in a row, we'll needlessly
    // thrash malloc. When we finally finish the parse the
    // HTMLToken will be destroyed and the VectorBuffer released.
    data_.Shrink(0);
    or_all_data_ = 0;
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

  bool IsAll8BitData() const { return (or_all_data_ <= 0xff); }

  const DataVector& GetName() const {
    DCHECK(type_ == kStartTag || type_ == kEndTag || type_ == DOCTYPE);
    return data_;
  }

  void AppendToName(UChar character) {
    DCHECK(type_ == kStartTag || type_ == kEndTag || type_ == DOCTYPE);
    DCHECK(character);
    data_.push_back(character);
    or_all_data_ |= character;
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
    data_.push_back(character);
    or_all_data_ |= character;
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
    current_attribute_ = nullptr;
    attributes_.clear();

    data_.push_back(character);
    or_all_data_ |= character;
  }

  void BeginEndTag(LChar character) {
    DCHECK_EQ(type_, kUninitialized);
    type_ = kEndTag;
    self_closing_ = false;
    current_attribute_ = nullptr;
    attributes_.clear();

    data_.push_back(character);
  }

  void BeginEndTag(const Vector<LChar, 32>& characters) {
    DCHECK_EQ(type_, kUninitialized);
    type_ = kEndTag;
    self_closing_ = false;
    current_attribute_ = nullptr;
    attributes_.clear();

    data_.AppendVector(characters);
  }

  void AddNewAttribute() {
    DCHECK(type_ == kStartTag || type_ == kEndTag);
    attributes_.Grow(attributes_.size() + 1);
    current_attribute_ = &attributes_.back();
    current_attribute_->MutableNameRange().Clear();
    current_attribute_->MutableValueRange().Clear();
  }

  void BeginAttributeName(int offset) {
    current_attribute_->MutableNameRange().start = offset - base_offset_;
    current_attribute_->NameRange().CheckValidStart();
  }

  void EndAttributeName(int offset) {
    int index = offset - base_offset_;
    current_attribute_->MutableNameRange().end = index;
    current_attribute_->NameRange().CheckValid();
    current_attribute_->MutableValueRange().start = index;
    current_attribute_->MutableValueRange().end = index;
  }

  void BeginAttributeValue(int offset) {
    current_attribute_->MutableValueRange().Clear();
    current_attribute_->MutableValueRange().start = offset - base_offset_;
    current_attribute_->ValueRange().CheckValidStart();
  }

  void EndAttributeValue(int offset) {
    current_attribute_->MutableValueRange().end = offset - base_offset_;
    current_attribute_->ValueRange().CheckValid();
  }

  void AppendToAttributeName(UChar character) {
    DCHECK(character);
    DCHECK(type_ == kStartTag || type_ == kEndTag);
    current_attribute_->NameRange().CheckValidStart();
    current_attribute_->AppendToName(character);
  }

  void AppendToAttributeValue(UChar character) {
    DCHECK(character);
    DCHECK(type_ == kStartTag || type_ == kEndTag);
    current_attribute_->ValueRange().CheckValidStart();
    current_attribute_->AppendToValue(character);
  }

  void AppendToAttributeValue(wtf_size_t i, const String& value) {
    DCHECK(!value.IsEmpty());
    DCHECK(type_ == kStartTag || type_ == kEndTag);
    attributes_[i].AppendToValue(value);
  }

  const AttributeList& Attributes() const {
    DCHECK(type_ == kStartTag || type_ == kEndTag);
    return attributes_;
  }

  const Attribute* GetAttributeItem(const QualifiedName& name) const {
    for (unsigned i = 0; i < attributes_.size(); ++i) {
      if (attributes_.at(i).GetName() == name.LocalName())
        return &attributes_.at(i);
    }
    return nullptr;
  }

  // Used by the XSSAuditor to nuke XSS-laden attributes.
  void EraseValueOfAttribute(wtf_size_t i) {
    DCHECK(type_ == kStartTag || type_ == kEndTag);
    attributes_[i].ClearValue();
  }

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
    data_.push_back(character);
  }

  void AppendToCharacter(UChar character) {
    DCHECK_EQ(type_, kCharacter);
    data_.push_back(character);
    or_all_data_ |= character;
  }

  void AppendToCharacter(const Vector<LChar, 32>& characters) {
    DCHECK_EQ(type_, kCharacter);
    data_.AppendVector(characters);
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
    data_.push_back(character);
    or_all_data_ |= character;
  }

  // Only for XSSAuditor
  void EraseCharacters() {
    DCHECK_EQ(type_, kCharacter);
    data_.clear();
    or_all_data_ = 0;
  }

 private:
  TokenType type_;
  Attribute::Range range_;  // Always starts at zero.
  int base_offset_;
  DataVector data_;
  UChar or_all_data_;

  // For StartTag and EndTag
  bool self_closing_;
  AttributeList attributes_;

  // A pointer into attributes_ used during lexing.
  Attribute* current_attribute_;

  // For DOCTYPE
  std::unique_ptr<DoctypeData> doctype_data_;

  DISALLOW_COPY_AND_ASSIGN(HTMLToken);
};

#ifndef NDEBUG
const char* ToString(HTMLToken::TokenType);
#endif

}  // namespace blink

#endif
