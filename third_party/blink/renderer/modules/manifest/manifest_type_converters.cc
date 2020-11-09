// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_type_converters.h"

#include <utility>

#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_mojom_traits.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {

blink::Manifest
TypeConverter<blink::Manifest, blink::mojom::blink::ManifestPtr>::Convert(
    const blink::mojom::blink::ManifestPtr& input) {
  blink::Manifest output;
  if (input.is_null())
    return output;

  if (!input->name.IsEmpty())
    output.name = blink::WebString(input->name).Utf16();

  if (!input->short_name.IsEmpty())
    output.short_name = blink::WebString(input->short_name).Utf16();

  if (!input->start_url.IsEmpty())
    output.start_url = input->start_url;

  output.display = input->display;
  output.orientation = input->orientation;

  for (auto& icon : input->icons)
    output.icons.push_back(icon.To<blink::Manifest::ImageResource>());

  for (auto& shortcut : input->shortcuts)
    output.shortcuts.push_back(shortcut.To<blink::Manifest::ShortcutItem>());

  if (!input->share_target.is_null()) {
    output.share_target =
        input->share_target.To<blink::Manifest::ShareTarget>();
  }

  for (auto& entry : input->file_handlers) {
    output.file_handlers.push_back(entry.To<blink::Manifest::FileHandler>());
  }

  for (auto& uri_protocol : input->protocol_handlers) {
    output.protocol_handlers.push_back(
        uri_protocol.To<blink::Manifest::ProtocolHandler>());
  }

  for (auto& url_handler : input->url_handlers) {
    output.url_handlers.push_back(
        url_handler.To<blink::Manifest::UrlHandler>());
  }

  for (auto& related_application : input->related_applications) {
    output.related_applications.push_back(
        related_application.To<blink::Manifest::RelatedApplication>());
  }

  output.prefer_related_applications = input->prefer_related_applications;

  if (input->has_theme_color)
    output.theme_color = input->theme_color;

  if (input->has_background_color)
    output.background_color = input->background_color;

  if (!input->gcm_sender_id.IsEmpty()) {
    output.gcm_sender_id = blink::WebString(input->gcm_sender_id).Utf16();
  }

  if (!input->scope.IsEmpty())
    output.scope = input->scope;

  return output;
}

blink::Manifest::ImageResource
TypeConverter<blink::Manifest::ImageResource,
              blink::mojom::blink::ManifestImageResourcePtr>::
    Convert(const blink::mojom::blink::ManifestImageResourcePtr& input) {
  blink::Manifest::ImageResource output;
  if (input.is_null())
    return output;

  output.src = input->src;
  output.type = blink::WebString(input->type).Utf16();

  for (auto& size : input->sizes)
    output.sizes.push_back(gfx::Size(size));

  for (auto purpose : input->purpose) {
    blink::mojom::ManifestImageResource_Purpose out_purpose;
    if (!EnumTraits<blink::mojom::ManifestImageResource_Purpose,
                    ::blink::mojom::ManifestImageResource_Purpose>::
            FromMojom(purpose, &out_purpose)) {
      NOTREACHED();
    }
    output.purpose.push_back(out_purpose);
  }

  return output;
}

blink::Manifest::ShortcutItem
TypeConverter<blink::Manifest::ShortcutItem,
              blink::mojom::blink::ManifestShortcutItemPtr>::
    Convert(const blink::mojom::blink::ManifestShortcutItemPtr& input) {
  blink::Manifest::ShortcutItem output;
  if (input.is_null())
    return output;

  output.name = blink::WebString(input->name).Utf16();

  if (!input->short_name.IsEmpty()) {
    output.short_name = blink::WebString(input->short_name).Utf16();
  }

  if (!input->description.IsEmpty()) {
    output.description = blink::WebString(input->description).Utf16();
  }

  output.url = input->url;

  for (auto& icon : input->icons)
    output.icons.push_back(icon.To<::blink::Manifest::ImageResource>());

  return output;
}

blink::Manifest::ShareTarget
TypeConverter<blink::Manifest::ShareTarget,
              blink::mojom::blink::ManifestShareTargetPtr>::
    Convert(const blink::mojom::blink::ManifestShareTargetPtr& input) {
  blink::Manifest::ShareTarget output;
  if (input.is_null())
    return output;

  output.action = input->action;
  output.method = input->method;
  output.enctype = input->enctype;

  output.params = input->params.To<::blink::Manifest::ShareTargetParams>();

  return output;
}

blink::Manifest::ShareTargetParams
TypeConverter<blink::Manifest::ShareTargetParams,
              blink::mojom::blink::ManifestShareTargetParamsPtr>::
    Convert(const blink::mojom::blink::ManifestShareTargetParamsPtr& input) {
  blink::Manifest::ShareTargetParams output;
  if (input.is_null())
    return output;

  if (!input->title.IsEmpty()) {
    output.title = blink::WebString(input->title).Utf16();
  }

  if (!input->text.IsEmpty())
    output.text = blink::WebString(input->text).Utf16();

  if (!input->url.IsEmpty())
    output.url = blink::WebString(input->url).Utf16();

  if (input->files.has_value()) {
    for (auto& file : *input->files)
      output.files.push_back(file.To<::blink::Manifest::FileFilter>());
  }

  return output;
}

blink::Manifest::FileFilter
TypeConverter<blink::Manifest::FileFilter,
              blink::mojom::blink::ManifestFileFilterPtr>::
    Convert(const blink::mojom::blink::ManifestFileFilterPtr& input) {
  blink::Manifest::FileFilter output;
  if (input.is_null())
    return output;

  output.name = blink::WebString(input->name).Utf16();

  for (auto& accept : input->accept)
    output.accept.push_back(blink::WebString(accept).Utf16());

  return output;
}

blink::Manifest::FileHandler
TypeConverter<blink::Manifest::FileHandler,
              blink::mojom::blink::ManifestFileHandlerPtr>::
    Convert(const blink::mojom::blink::ManifestFileHandlerPtr& input) {
  blink::Manifest::FileHandler output;
  if (input.is_null())
    return output;

  output.name = blink::WebString(input->name).Utf16();
  output.action = input->action;
  for (const auto& it : input->accept) {
    auto& extensions = output.accept[blink::WebString(it.key).Utf16()];
    for (const auto& extension : it.value)
      extensions.push_back(blink::WebString(extension).Utf16());
  }

  return output;
}

blink::Manifest::ProtocolHandler
TypeConverter<blink::Manifest::ProtocolHandler,
              blink::mojom::blink::ManifestProtocolHandlerPtr>::
    Convert(const blink::mojom::blink::ManifestProtocolHandlerPtr& input) {
  blink::Manifest::ProtocolHandler output;
  if (input.is_null())
    return output;
  output.protocol = blink::WebString(input->protocol).Utf16();
  output.url = input->url;
  return output;
}

blink::Manifest::UrlHandler
TypeConverter<blink::Manifest::UrlHandler,
              blink::mojom::blink::ManifestUrlHandlerPtr>::
    Convert(const blink::mojom::blink::ManifestUrlHandlerPtr& input) {
  blink::Manifest::UrlHandler output;
  if (input.is_null())
    return output;

  if (!output.origin.opaque())
    output.origin = input->origin->ToUrlOrigin();

  return output;
}

blink::Manifest::RelatedApplication
TypeConverter<blink::Manifest::RelatedApplication,
              blink::mojom::blink::ManifestRelatedApplicationPtr>::
    Convert(const blink::mojom::blink::ManifestRelatedApplicationPtr& input) {
  blink::Manifest::RelatedApplication output;
  if (input.is_null())
    return output;

  if (!input->platform.IsEmpty()) {
    output.platform = blink::WebString(input->platform).Utf16();
  }

  if (input->url.has_value())
    output.url = *input->url;

  if (!input->id.IsEmpty())
    output.id = blink::WebString(input->id).Utf16();

  return output;
}

}  // namespace mojo
