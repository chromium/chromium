/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights
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

#include "third_party/blink/renderer/core/style/nine_piece_image.h"

#include "base/memory/values_equivalent.h"

namespace blink {

static scoped_refptr<NinePieceImageData>& DefaultData() {
  static scoped_refptr<NinePieceImageData>* data = nullptr;
  if (!data) {
    data = new scoped_refptr<NinePieceImageData>(NinePieceImageData::Create());
  }
  return *data;
}

NinePieceImage::NinePieceImage() : data_(DefaultData()) {}

NinePieceImage::NinePieceImage(StyleImage* image,
                               LengthBox image_slices,
                               bool fill,
                               const BorderImageLengthBox& border_slices,
                               const BorderImageLengthBox& outset,
                               ENinePieceImageRule horizontal_rule,
                               ENinePieceImageRule vertical_rule) {
  data_ = NinePieceImageData::Create();
  Access()->image = image;
  Access()->image_slices = image_slices;
  Access()->border_slices = border_slices;
  Access()->outset = outset;
  Access()->fill = fill;
  Access()->horizontal_rule = horizontal_rule;
  Access()->vertical_rule = vertical_rule;
}

NinePieceImageData::NinePieceImageData()
    : fill(false),
      horizontal_rule(kStretchImageRule),
      vertical_rule(kStretchImageRule),
      image(nullptr),
      image_slices(Length::Percent(100),
                   Length::Percent(100),
                   Length::Percent(100),
                   Length::Percent(100)),
      border_slices(1.0, 1.0, 1.0, 1.0),
      outset(0, 0, 0, 0) {}

bool NinePieceImageData::operator==(const NinePieceImageData& other) const {
  return base::ValuesEquivalent(image, other.image) &&
         image_slices == other.image_slices && fill == other.fill &&
         border_slices == other.border_slices && outset == other.outset &&
         horizontal_rule == other.horizontal_rule &&
         vertical_rule == other.vertical_rule;
}

}  // namespace blink
