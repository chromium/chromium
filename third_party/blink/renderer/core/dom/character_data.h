/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights
 * reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHARACTER_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHARACTER_DATA_H_

#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT CharacterData : public Node {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Makes the data Parkable. This enables de-duplication and compression.
  void MakeParkable();
  const String& data() const {
    if (auto* parked = absl::get_if<ParkableString>(&data_))
      return parked->ToString();
    return absl::get<String>(data_);
  }
  void setData(const String&);
  unsigned length() const {
    if (auto* parked = absl::get_if<ParkableString>(&data_))
      return parked->length();
    return absl::get<String>(data_).length();
  }
  String substringData(unsigned offset, unsigned count, ExceptionState&);
  void appendData(const String&);
  void replaceData(unsigned offset,
                   unsigned count,
                   const String&,
                   ExceptionState&);

  void insertData(unsigned offset, const String&, ExceptionState&);
  void deleteData(unsigned offset, unsigned count, ExceptionState&);

  bool ContainsOnlyWhitespaceOrEmpty() const;

  StringImpl* DataImpl() { return data().Impl(); }

  void ParserAppendData(const String&);

 protected:
  CharacterData(TreeScope& tree_scope,
                const String& text,
                ConstructionType type)
      : Node(&tree_scope, type), data_(!text.IsNull() ? text : g_empty_string) {
    DCHECK(type == kCreateComment || type == kCreateText ||
           type == kCreateCdataSection ||
           type == kCreateProcessingInstruction || type == kCreateEditingText);
  }

  CharacterData(TreeScope& tree_scope, String&& text, ConstructionType type)
      : Node(&tree_scope, type), data_(std::move(text)) {
    DCHECK(type == kCreateComment || type == kCreateText ||
           type == kCreateCdataSection ||
           type == kCreateProcessingInstruction || type == kCreateEditingText);
    DCHECK(absl::holds_alternative<String>(data_));
    if (absl::get<String>(data_).IsNull()) {
      data_ = g_empty_string;
    }
  }

  void SetDataWithoutUpdate(const String& data) {
    DCHECK(!data.IsNull());
    data_ = data;
  }
  enum UpdateSource {
    kUpdateFromParser,
    kUpdateFromNonParser,
  };
  void DidModifyData(const String& old_value, UpdateSource);

  absl::variant<ParkableString, String> data_;

 private:
  String nodeValue() const final;
  void setNodeValue(const String&, ExceptionState&) final;
  bool IsCharacterDataNode() const final { return true; }
  void SetDataAndUpdate(const String&,
                        unsigned offset_of_replaced_data,
                        unsigned old_length,
                        unsigned new_length,
                        UpdateSource = kUpdateFromNonParser);

  bool IsContainerNode() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsElementNode() const =
      delete;  // This will catch anyone doing an unnecessary check.
};

template <>
struct DowncastTraits<CharacterData> {
  static bool AllowFrom(const Node& node) { return node.IsCharacterDataNode(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHARACTER_DATA_H_
