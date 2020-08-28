// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_METADATA_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_METADATA_H_

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ScriptState;
class ScriptPromise;
class ScriptPromiseResolver;
struct FontEnumerationEntry;

class BLINK_EXPORT FontMetadata final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit FontMetadata(const FontEnumerationEntry& entry);

  static FontMetadata* Create(const FontEnumerationEntry& entry);

  // The table below represents the properties made available via the API, the
  // name table entries those properties map to, and their localization status
  // as returned by the API.
  // The localized properties are in the system's configured locale.
  //
  // For more about name table entries, go to:
  // https://docs.microsoft.com/en-us/typography/opentype/spec/name#name-ids
  //
  //  +----------------+---------+-----------+
  //  |    Property    | name ID | Localized |
  //  +----------------+---------+-----------+
  //  | postscriptName |       6 | No        |
  //  | fullName       |       4 | Yes       |
  //  | family         |       1 | Yes       |
  //  +----------------+---------+-----------+

  String postscriptName() const { return postscriptName_; }
  String fullName() const { return fullName_; }
  String family() const { return family_; }

  ScriptPromise blob(ScriptState*);

  void Trace(Visitor*) const override;

 private:
  static void BlobImpl(ScriptPromiseResolver* resolver,
                       const String& postscriptName);
  String postscriptName_;
  String fullName_;
  String family_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_METADATA_H_
