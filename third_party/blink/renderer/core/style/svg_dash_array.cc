/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2010 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    Based on khtml code by:
    Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
    Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
    Copyright (C) 2002-2003 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "third_party/blink/renderer/core/style/svg_dash_array.h"

#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

SVGDashArray* EmptyDashArray() {
  DEFINE_STATIC_REF(SVGDashArray, empty_dash_array,
                    base::MakeRefCounted<SVGDashArray>());
  return empty_dash_array;
}

}  // namespace blink
