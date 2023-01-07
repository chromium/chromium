/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/text/web_entities.h"

#include <string.h>
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

WebEntities::WebEntities(bool xml_entities) {
  DCHECK(entities_map_.empty());
  entities_map_.Set(0x003c, "lt");
  entities_map_.Set(0x003e, "gt");
  entities_map_.Set(0x0026, "amp");
  entities_map_.Set(0x0027, "apos");
  entities_map_.Set(0x0022, "quot");
  // We add #39 for test-compatibility reason.
  if (!xml_entities)
    entities_map_.Set(0x0027, String("#39"));
}

String WebEntities::EntityNameByCode(int code) const {
  // FIXME: We should use find so we only do one hash lookup.
  if (entities_map_.Contains(code))
    return entities_map_.at(code);
  return "";
}

String WebEntities::ConvertEntitiesInString(const String& value) const {
  StringBuilder result;
  bool did_convert_entity = false;
  unsigned length = value.length();
  for (unsigned i = 0; i < length; ++i) {
    UChar c = value[i];
    // FIXME: We should use find so we only do one hash lookup.
    if (entities_map_.Contains(c)) {
      did_convert_entity = true;
      result.Append('&');
      result.Append(entities_map_.at(c));
      result.Append(';');
    } else {
      result.Append(c);
    }
  }

  if (!did_convert_entity)
    return value;

  return result.ToString();
}

}  // namespace blink
