/* ***** BEGIN LICENSE BLOCK *****
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ***** END LICENSE BLOCK ***** */

#include <hb-ot.h>

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_caps_support.h"

namespace blink {

bool OpenTypeCapsSupport::SupportsOpenTypeFeature(hb_script_t script,
                                                  uint32_t tag) const {
  hb_face_t* const face = hb_font_get_face(harfbuzz_face_->GetScaledFont());
  DCHECK(face);

  DCHECK(
      (tag == HB_TAG('s', 'm', 'c', 'p') || tag == HB_TAG('c', '2', 's', 'c') ||
       tag == HB_TAG('p', 'c', 'a', 'p') || tag == HB_TAG('c', '2', 'p', 'c') ||
       tag == HB_TAG('s', 'u', 'p', 's') || tag == HB_TAG('s', 'u', 'b', 's') ||
       tag == HB_TAG('t', 'i', 't', 'l') || tag == HB_TAG('u', 'n', 'i', 'c') ||
       tag == HB_TAG('v', 'e', 'r', 't')));

  if (!hb_ot_layout_has_substitution(face))
    return false;

  // Get the OpenType tag(s) that match this script code
  DCHECK_EQ(HB_TAG_NONE, 0u);
  hb_tag_t script_tags[2] = {};
  unsigned num_returned_script_tags = std::size(script_tags);
  hb_ot_tags_from_script_and_language(
      static_cast<hb_script_t>(script), HB_LANGUAGE_INVALID,
      &num_returned_script_tags, script_tags, nullptr, nullptr);

  const hb_tag_t kGSUB = HB_TAG('G', 'S', 'U', 'B');
  unsigned script_index = 0;
  // Identify for which script a GSUB table is available.
  hb_ot_layout_table_select_script(face, kGSUB, num_returned_script_tags,
                                   script_tags, &script_index, nullptr);

  if (hb_ot_layout_language_find_feature(face, kGSUB, script_index,
                                         HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX,
                                         tag, nullptr)) {
    return true;
  }
  return false;
}

}  // namespace blink
