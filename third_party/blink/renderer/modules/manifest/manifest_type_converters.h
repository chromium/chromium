// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_TYPE_CONVERTERS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_TYPE_CONVERTERS_H_

#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {
struct Manifest;
}

namespace mojo {

// TODO(crbug.com/704441): These converters are temporary to help the manifest
// migration until content::PushMessagingClient is moved to blink.

template <>
struct MODULES_EXPORT
    TypeConverter<blink::Manifest, blink::mojom::blink::ManifestPtr> {
  static blink::Manifest Convert(const blink::mojom::blink::ManifestPtr& input);
};

template <>
struct TypeConverter<blink::Manifest::ImageResource,
                     blink::mojom::blink::ManifestImageResourcePtr> {
  static blink::Manifest::ImageResource Convert(
      const blink::mojom::blink::ManifestImageResourcePtr& input);
};

template <>
struct TypeConverter<blink::Manifest::ShortcutItem,
                     blink::mojom::blink::ManifestShortcutItemPtr> {
  static blink::Manifest::ShortcutItem Convert(
      const blink::mojom::blink::ManifestShortcutItemPtr& input);
};

template <>
struct TypeConverter<blink::Manifest::ShareTarget,
                     blink::mojom::blink::ManifestShareTargetPtr> {
  static blink::Manifest::ShareTarget Convert(
      const blink::mojom::blink::ManifestShareTargetPtr& input);
};

template <>
struct TypeConverter<blink::Manifest::ShareTargetParams,
                     blink::mojom::blink::ManifestShareTargetParamsPtr> {
  static blink::Manifest::ShareTargetParams Convert(
      const blink::mojom::blink::ManifestShareTargetParamsPtr& input);
};

template <>
struct TypeConverter<blink::Manifest::FileFilter,
                     blink::mojom::blink::ManifestFileFilterPtr> {
  static blink::Manifest::FileFilter Convert(
      const blink::mojom::blink::ManifestFileFilterPtr& input);
};

template <>
struct TypeConverter<blink::Manifest::FileHandler,
                     blink::mojom::blink::ManifestFileHandlerPtr> {
  static blink::Manifest::FileHandler Convert(
      const blink::mojom::blink::ManifestFileHandlerPtr& input);
};

template <>
struct TypeConverter<blink::Manifest::ProtocolHandler,
                     blink::mojom::blink::ManifestProtocolHandlerPtr> {
  static blink::Manifest::ProtocolHandler Convert(
      const blink::mojom::blink::ManifestProtocolHandlerPtr& input);
};

template <>
struct TypeConverter<blink::Manifest::UrlHandler,
                     blink::mojom::blink::ManifestUrlHandlerPtr> {
  static blink::Manifest::UrlHandler Convert(
      const blink::mojom::blink::ManifestUrlHandlerPtr& input);
};

template <>
struct TypeConverter<blink::Manifest::RelatedApplication,
                     blink::mojom::blink::ManifestRelatedApplicationPtr> {
  static blink::Manifest::RelatedApplication Convert(
      const blink::mojom::blink::ManifestRelatedApplicationPtr& input);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_TYPE_CONVERTERS_H_
