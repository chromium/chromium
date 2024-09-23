/*
 * Copyright (C) 2003, 2006, 2008, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/utf16_text_iterator.h"

namespace blink {

bool UTF16TextIterator::IsValidSurrogatePair(UChar32& character) {
  // If we have a surrogate pair, make sure it starts with the high part.
  if (!U16_IS_SURROGATE_LEAD(character))
    return false;

  // Do we have a surrogate pair? If so, determine the full Unicode (32 bit)
  // code point before glyph lookup.
  // Make sure we have another character and it's a low surrogate.
  if (characters_ + 1 >= characters_end_)
    return false;

  UChar low = characters_[1];
  if (!U16_IS_TRAIL(low))
    return false;
  return true;
}

bool UTF16TextIterator::ConsumeSurrogatePair(UChar32& character) {
  DCHECK(U16_IS_SURROGATE(character));

  if (!IsValidSurrogatePair(character)) {
    character = WTF::unicode::kReplacementCharacter;
    return true;
  }

  UChar low = characters_[1];
  character = U16_GET_SUPPLEMENTARY(character, low);
  current_glyph_length_ = 2;
  return true;
}

}  // namespace blink
