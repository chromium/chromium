// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_COMMON_METAFILE_UTILS_H_
#define PRINTING_COMMON_METAFILE_UTILS_H_

#include <stdint.h>

#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/types/strong_alias.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "printing/mojom/print.mojom-forward.h"
#include "third_party/skia/include/core/SkDocument.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "ui/accessibility/ax_tree_update_forward.h"

class SkWStream;

namespace printing {

using ContentToProxyTokenMap = base::flat_map<uint32_t, base::UnguessableToken>;

// Identifies serialized content (remote subframes, typefaces, or images) within
// a document.
using PictureContentId = base::StrongAlias<class PictureContentIdTag, uint32_t>;
using TypefaceContentId =
    base::StrongAlias<class TypefaceContentIdTag, uint32_t>;
using ImageContentId = base::StrongAlias<class ImageContentIdTag, uint64_t>;

// Stores the mapping between a content id and its actual content.
using PictureDeserializationContext =
    base::flat_map<PictureContentId, sk_sp<SkPicture>>;
using TypefaceDeserializationContext =
    base::flat_map<TypefaceContentId, sk_sp<SkTypeface>>;
using ImageDeserializationContext =
    base::flat_map<ImageContentId, sk_sp<SkImage>>;

// Stores the mapping between content's unique id and its corresponding frame
// proxy token.
using PictureSerializationContext = ContentToProxyTokenMap;

// Stores the set of typeface unique ids used by the picture frame content.
using TypefaceSerializationContext = base::flat_set<TypefaceContentId>;

// Stores the set of serialized image ids used by the content.
using ImageSerializationContext = base::flat_set<ImageContentId>;

sk_sp<SkDocument> MakePdfDocument(
    std::string_view creator,
    std::string_view title,
    const ui::AXTreeUpdate& accessibility_tree,
    mojom::GenerateDocumentOutline generate_document_outline,
    SkWStream* stream);

SkSerialProcs SerializationProcs(PictureSerializationContext* picture_ctx,
                                 TypefaceSerializationContext* typeface_ctx,
                                 ImageSerializationContext* image_ctx);

SkDeserialProcs DeserializationProcs(
    PictureDeserializationContext* picture_ctx,
    TypefaceDeserializationContext* typeface_ctx,
    ImageDeserializationContext* image_ctx);

}  // namespace printing

#endif  // PRINTING_COMMON_METAFILE_UTILS_H_
