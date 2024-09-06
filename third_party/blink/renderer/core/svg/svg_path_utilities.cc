/*
 * Copyright (C) Research In Motion Limited 2010, 2012. All rights reserved.
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
 */

#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"

#include "third_party/blink/renderer/core/svg/svg_path_builder.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_builder.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_source.h"
#include "third_party/blink/renderer/core/svg/svg_path_parser.h"
#include "third_party/blink/renderer/core/svg/svg_path_string_builder.h"
#include "third_party/blink/renderer/core/svg/svg_path_string_source.h"

namespace blink {

bool BuildPathFromString(const StringView& path_string, Path& result) {
  if (path_string.empty())
    return true;

  SVGPathBuilder builder(result);
  SVGPathStringSource source(path_string);
  return svg_path_parser::ParsePath(source, builder);
}

bool BuildPathFromByteStream(const SVGPathByteStream& stream, Path& result) {
  if (stream.IsEmpty())
    return true;

  SVGPathBuilder builder(result);
  SVGPathByteStreamSource source(stream);
  return svg_path_parser::ParsePath(source, builder);
}

String BuildStringFromByteStream(const SVGPathByteStream& stream,
                                 PathSerializationFormat format) {
  if (stream.IsEmpty())
    return String();

  SVGPathStringBuilder builder;
  SVGPathByteStreamSource source(stream);
  if (format == kTransformToAbsolute) {
    SVGPathAbsolutizer absolutizer(&builder);
    svg_path_parser::ParsePath(source, absolutizer);
  } else {
    svg_path_parser::ParsePath(source, builder);
  }
  return builder.Result();
}

SVGParsingError BuildByteStreamFromString(const StringView& path_string,
                                          SVGPathByteStreamBuilder& builder) {
  if (path_string.empty())
    return SVGParseStatus::kNoError;

  // The string length is typically a minor overestimate of eventual byte stream
  // size, so it avoids us a lot of reallocs.
  builder.ReserveInitialCapacity(path_string.length());

  SVGPathStringSource source(path_string);
  svg_path_parser::ParsePath(source, builder);
  return source.ParseError();
}

}  // namespace blink
