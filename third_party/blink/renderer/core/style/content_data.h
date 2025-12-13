/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_CONTENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_CONTENT_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class LayoutObject;
class TreeScope;
class StyleEngine;
class CountersAttachmentContext;

class ContentData : public GarbageCollected<ContentData> {
 public:
  virtual ~ContentData() = default;

  virtual bool IsCounter() const { return false; }
  virtual bool IsAltCounter() const { return false; }
  virtual bool IsImage() const { return false; }
  virtual bool IsQuote() const { return false; }
  virtual bool IsText() const { return false; }
  virtual bool IsAltText() const { return false; }
  virtual bool IsNone() const { return false; }
  virtual bool IsAlt() const { return IsAltText() || IsAltCounter(); }

  // Returns true if this content data list contains any AltCounterContentData.
  bool HasAltCounterContent() const;

  CORE_EXPORT static String ConcatenateAltText(
      const ContentData& first_alt_data);

  // Create a layout object for this piece of content. `owner` is the layout
  // object that has the content property, e.g. a pseudo-element, or an @page
  // margin box.
  virtual LayoutObject* CreateLayoutObject(LayoutObject& owner) const = 0;

  virtual ContentData* Clone() const;

  ContentData* Next() const { return next_.Get(); }
  void SetNext(ContentData* next) {
    DCHECK(!IsNone());
    DCHECK(!next || !next->IsNone());
    next_ = next;
  }

  virtual bool Equals(const ContentData&) const = 0;

  virtual void Trace(Visitor*) const;

  // For debugging/logging only.
  virtual String DebugString() const { return "<unknown>"; }

 private:
  virtual ContentData* CloneInternal() const = 0;

  Member<ContentData> next_;

  friend std::ostream& operator<<(std::ostream& stream,
                                  const ContentData& content_data);
};

inline std::ostream& operator<<(std::ostream& stream,
                                const ContentData& content_data) {
  const ContentData* ptr = &content_data;
  stream << "ContentData{";
  while (ptr) {
    stream << content_data.DebugString();
    stream << ",";
    ptr = ptr->next_.Get();
  }
  return stream << "}";
}

class ImageContentData final : public ContentData {
 public:
  explicit ImageContentData(StyleImage* image) : image_(image) {
    DCHECK(image_);
  }

  const StyleImage* GetImage() const { return image_.Get(); }
  StyleImage* GetImage() { return image_.Get(); }
  void SetImage(StyleImage* image) {
    DCHECK(image);
    image_ = image;
  }

  bool IsImage() const override { return true; }
  LayoutObject* CreateLayoutObject(LayoutObject& owner) const override;

  bool Equals(const ContentData& data) const override {
    if (!data.IsImage()) {
      return false;
    }
    return *static_cast<const ImageContentData&>(data).GetImage() ==
           *GetImage();
  }

  void Trace(Visitor*) const override;

  String DebugString() const override {
    StringBuilder str;
    str.Append("<image: ");
    if (image_->IsImageResource()) {
      str.Append("[is_resource]");
    }
    if (image_->IsPendingImage()) {
      str.Append("[pending]");
    }
    if (image_->IsGeneratedImage()) {
      str.Append("[generated]");
    }
    if (image_->IsContentful()) {
      str.Append("[contentful]");
    }
    if (image_->IsImageResourceSet()) {
      str.Append("[resourceset]");
    }
    if (image_->IsPaintImage()) {
      str.Append("[paint]");
    }
    if (image_->IsCrossfadeImage()) {
      str.Append("[crossfade]");
    }
    str.Append(image_->CssValue()->CssText());
    str.Append(">");
    return str.ReleaseString();
  }

 private:
  ContentData* CloneInternal() const override {
    StyleImage* image = const_cast<StyleImage*>(GetImage());
    return MakeGarbageCollected<ImageContentData>(image);
  }

  Member<StyleImage> image_;
};

template <>
struct DowncastTraits<ImageContentData> {
  static bool AllowFrom(const ContentData& content) {
    return content.IsImage();
  }
};

class TextContentData final : public ContentData {
 public:
  explicit TextContentData(const String& text) : text_(text) {}

  const String& GetText() const { return text_; }
  void SetText(const String& text) { text_ = text; }

  bool IsText() const override { return true; }
  LayoutObject* CreateLayoutObject(LayoutObject& owner) const override;

  bool Equals(const ContentData& data) const override {
    if (!data.IsText()) {
      return false;
    }
    return static_cast<const TextContentData&>(data).GetText() == GetText();
  }

  String DebugString() const override { return text_; }

 private:
  ContentData* CloneInternal() const override {
    return MakeGarbageCollected<TextContentData>(GetText());
  }

  String text_;
};

template <>
struct DowncastTraits<TextContentData> {
  static bool AllowFrom(const ContentData& content) { return content.IsText(); }
};

class AltTextContentData final : public ContentData {
 public:
  explicit AltTextContentData(const String& text) : text_(text) {}

  String GetText() const { return text_; }
  void SetText(const String& text) { text_ = text; }

  bool IsAltText() const override { return true; }
  LayoutObject* CreateLayoutObject(LayoutObject& owner) const override;

  bool Equals(const ContentData& data) const override {
    if (!data.IsAltText()) {
      return false;
    }
    return static_cast<const AltTextContentData&>(data).GetText() == GetText();
  }

  String DebugString() const override { return StrCat({"<alt: ", text_, ">"}); }

 private:
  ContentData* CloneInternal() const override {
    return MakeGarbageCollected<AltTextContentData>(GetText());
  }
  String text_;
};

template <>
struct DowncastTraits<AltTextContentData> {
  static bool AllowFrom(const ContentData& content) {
    return content.IsAltText();
  }
};

struct CounterData {
  DISALLOW_NEW();

 public:
  CounterData(const AtomicString& identifier,
              const AtomicString& style,
              const AtomicString& separator,
              const TreeScope* tree_scope)
      : identifier(identifier),
        list_style(style),
        separator(separator),
        tree_scope(tree_scope) {}

  void Trace(Visitor* v) const { v->Trace(tree_scope); }

  AtomicString identifier;
  AtomicString list_style;
  AtomicString separator;
  Member<const TreeScope> tree_scope;
};

class CounterContentData : public ContentData {
  friend class ContentData;

 public:
  CounterContentData(const AtomicString& identifier,
                     const AtomicString& style,
                     const AtomicString& separator,
                     const TreeScope* tree_scope)
      : counter_data_(identifier, style, separator, tree_scope) {}

  explicit CounterContentData(CounterData counter_data)
      : counter_data_(std::move(counter_data)) {}

  bool IsCounter() const override { return true; }
  LayoutObject* CreateLayoutObject(LayoutObject& owner) const override;

  const AtomicString& Identifier() const { return counter_data_.identifier; }
  const AtomicString& ListStyle() const { return counter_data_.list_style; }
  const AtomicString& Separator() const { return counter_data_.separator; }
  const TreeScope* GetTreeScope() const { return counter_data_.tree_scope; }

  void Trace(Visitor*) const override;

  String DebugString() const override { return "<counter>"; }

 private:
  ContentData* CloneInternal() const override {
    return MakeGarbageCollected<CounterContentData>(counter_data_);
  }

 protected:
  bool Equals(const ContentData& data) const override {
    if (!data.IsCounter()) {
      return false;
    }
    const CounterContentData& other =
        static_cast<const CounterContentData&>(data);
    return Identifier() == other.Identifier() &&
           ListStyle() == other.ListStyle() &&
           Separator() == other.Separator() &&
           GetTreeScope() == other.GetTreeScope();
  }

  CounterData counter_data_;
};

template <>
struct DowncastTraits<CounterContentData> {
  static bool AllowFrom(const ContentData& content) {
    return content.IsCounter();
  }
};

class AltCounterContentData : public CounterContentData {
  using CounterContentData::CounterContentData;

 public:
  bool IsAltCounter() const override { return true; }

  LayoutObject* CreateLayoutObject(LayoutObject& owner) const override;

  const String& GetText() const { return counter_value_text_; }
  void UpdateText(CountersAttachmentContext& context,
                  const StyleEngine& style_engine,
                  const LayoutObject& content_generating_object);

  String DebugString() const override { return "<alt-counter>"; }

 private:
  void SetText(const String& text) { counter_value_text_ = text; }

  ContentData* CloneInternal() const override {
    auto* data = MakeGarbageCollected<AltCounterContentData>(counter_data_);
    data->SetText(GetText());
    return data;
  }

  bool Equals(const ContentData& data) const override {
    if (!data.IsAltCounter()) {
      return false;
    }
    const AltCounterContentData& other =
        static_cast<const AltCounterContentData&>(data);
    return CounterContentData::Equals(other) && GetText() == other.GetText();
  }

  // Text value of counter() or counters() to be used in ax object.
  String counter_value_text_;
};

template <>
struct DowncastTraits<AltCounterContentData> {
  static bool AllowFrom(const ContentData& content) {
    return content.IsAltCounter();
  }
};

class QuoteContentData final : public ContentData {
 public:
  explicit QuoteContentData(QuoteType quote) : quote_(quote) {}

  QuoteType Quote() const { return quote_; }
  void SetQuote(QuoteType quote) { quote_ = quote; }

  bool IsQuote() const override { return true; }
  LayoutObject* CreateLayoutObject(LayoutObject& owner) const override;

  bool Equals(const ContentData& data) const override {
    if (!data.IsQuote()) {
      return false;
    }
    return static_cast<const QuoteContentData&>(data).Quote() == Quote();
  }

  String DebugString() const override { return "<quote>"; }

 private:
  ContentData* CloneInternal() const override {
    return MakeGarbageCollected<QuoteContentData>(Quote());
  }

  QuoteType quote_;
};

template <>
struct DowncastTraits<QuoteContentData> {
  static bool AllowFrom(const ContentData& content) {
    return content.IsQuote();
  }
};

class NoneContentData final : public ContentData {
 public:
  explicit NoneContentData() {}

  bool IsNone() const override { return true; }
  LayoutObject* CreateLayoutObject(LayoutObject& owner) const override;

  bool Equals(const ContentData& data) const override { return data.IsNone(); }

  String DebugString() const override { return "<none>"; }

 private:
  ContentData* CloneInternal() const override {
    return MakeGarbageCollected<NoneContentData>();
  }
};

template <>
struct DowncastTraits<NoneContentData> {
  static bool AllowFrom(const ContentData& content) { return content.IsNone(); }
};

inline bool operator==(const ContentData& a, const ContentData& b) {
  const ContentData* ptr_a = &a;
  const ContentData* ptr_b = &b;

  while (ptr_a && ptr_b && ptr_a->Equals(*ptr_b)) {
    ptr_a = ptr_a->Next();
    ptr_b = ptr_b->Next();
  }

  return !ptr_a && !ptr_b;
}

// In order for an image to be rendered from the content property on an actual
// element, there can be at most one piece of image content data, followed by
// some optional alternative text.
inline bool ShouldUseContentDataForElement(const ContentData* content_data) {
  if (!content_data) {
    return false;
  }
  if (!content_data->IsImage()) {
    return false;
  }
  if (content_data->Next() && !content_data->Next()->IsAlt()) {
    return false;
  }

  return true;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_CONTENT_DATA_H_
