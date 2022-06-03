// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_METADATA_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_METADATA_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ScriptState;
class ScriptPromise;
class ScriptPromiseResolver;

struct FontEnumerationEntry {
  String postscript_name;
  String full_name;
  String family;
  String style;
  bool italic;
  float stretch;
  float weight;
};

class BLINK_EXPORT FontMetadata final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit FontMetadata(const FontEnumerationEntry& entry);

  static FontMetadata* Create(const FontEnumerationEntry& entry);

  // The tables below represent the properties made available via the API.
  //
  // Names:
  //
  // This table shows the properties made available via the API, the name table
  // entries those properties map to, and their localization status as returned
  // by the API. The localized properties are in the system's configured locale.
  //
  // For more about name table entries, go to:
  // https://docs.microsoft.com/en-us/typography/opentype/spec/name#name-ids
  //
  //  +----------------+---------+-----------+
  //  |    Property    | name ID | Localized |
  //  +----------------+---------+-----------+
  //  | postscriptName |       6 | No        |
  //  | family         |       1 | No        |
  //  | style          |       2 | No        |
  //  | fullName       |       4 | Yes       |
  //  +----------------+---------+-----------+
  //
  // Other properties:
  //
  // This table shows the properties made available via the API, the "OS/2"
  // table entries those properties map to, additional details, and which CSS
  // properties these correspond to. Note that not all operating systems derive
  // the properties from these table entries.
  //
  // For more about "OS/2" table entries, see:
  // https://docs.microsoft.com/en-us/typography/opentype/spec/os2
  //
  //  +----------+---------------+------------------------+--------------+
  //  | Property |     Name      |        Details         |     CSS      |
  //  +----------+---------------+------------------------+--------------+
  //  | italic   | fsSelection   | uint16; bit 0 = italic | font-style   |
  //  | stretch  | usWidthClass  | uint16; 1 to 9         | font-stretch |
  //  | weight   | usWeightClass | uint16; 1 to 1000      | font-weight  |
  //  +----------+---------------+------------------------+--------------+
  //
  // In particular:
  //    * `italic` is true if the font is italic or oblique. In CSS this
  //        would be specified as e.g. font-style: italic.
  //    * `stretch` ranges from 0.5f (50% ultra-condensed) to 2.0f (200%
  //        ultra-expanded). In CSS, this would be specified as e.g.
  //        font-stretch: 100%.
  //    * `weight` ranges from 1 to 1000. 100 is extra-light, 400 is normal, 900
  //        is black. In CSS, this would be specified as e.g. font-weight: 400.

  String postscriptName() const { return postscriptName_; }
  String fullName() const { return fullName_; }
  String family() const { return family_; }
  String style() const { return style_; }
  bool italic() const { return italic_; }
  float stretch() const { return stretch_; }
  float weight() const { return weight_; }

  ScriptPromise blob(ScriptState*);

  void Trace(Visitor*) const override;

 private:
  static void BlobImpl(ScriptPromiseResolver* resolver,
                       const String& postscriptName);
  String postscriptName_;
  String fullName_;
  String family_;
  String style_;
  bool italic_;
  float stretch_;
  float weight_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_METADATA_H_
