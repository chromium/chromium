// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_COMMON_METAFILE_UTILS_H_
#define PRINTING_COMMON_METAFILE_UTILS_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/unguessable_token.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkDocument.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/accessibility/ax_tree_update_forward.h"

namespace printing {

using ContentToProxyTokenMap = base::flat_map<uint32_t, base::UnguessableToken>;
using ContentProxySet = base::flat_set<uint32_t>;

// Stores the mapping between a content's unique id and its actual content.
using PictureDeserializationContext =
    base::flat_map<uint32_t, sk_sp<SkPicture>>;
using TypefaceDeserializationContext =
    base::flat_map<uint32_t, sk_sp<SkTypeface>>;

// Stores the mapping between content's unique id and its corresponding frame
// proxy token.
using PictureSerializationContext = ContentToProxyTokenMap;

// Stores the set of typeface unique ids used by the picture frame content.
using TypefaceSerializationContext = ContentProxySet;

sk_sp<SkDocument> MakePdfDocument(const std::string& creator,
                                  const ui::AXTreeUpdate& accessibility_tree,
                                  SkWStream* stream);

SkSerialProcs SerializationProcs(PictureSerializationContext* picture_ctx,
                                 TypefaceSerializationContext* typeface_ctx);

SkDeserialProcs DeserializationProcs(
    PictureDeserializationContext* picture_ctx,
    TypefaceDeserializationContext* typeface_ctx);

}  // namespace printing

#endif  // PRINTING_COMMON_METAFILE_UTILS_H_
