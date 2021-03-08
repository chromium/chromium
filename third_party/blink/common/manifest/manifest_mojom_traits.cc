// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_mojom_traits.h"

#include <string>
#include <utility>

#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"
#include "url/url_util.h"

namespace mojo {
namespace {

// A wrapper around base::Optional<base::string16> so a custom StructTraits
// specialization can enforce maximum string length.
struct TruncatedString16 {
  base::Optional<base::string16> string;
};

// This function should be kept in sync with IsHostValidForUrlHandler in
// manifest_parser.cc.
bool IsHostValidForUrlHandler(const std::string& host) {
  if (url::HostIsIPAddress(host))
    return true;

  const size_t registry_length =
      net::registry_controlled_domains::PermissiveGetHostRegistryLength(
          host,
          // Reject unknown registries (registries that don't have any matches
          // in effective TLD names).
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          // Skip matching private registries that allow external users to
          // specify sub-domains, e.g. glitch.me, as this is allowed.
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);

  // Host cannot be a TLD or invalid.
  if (registry_length == 0 || registry_length == std::string::npos ||
      registry_length >= host.length()) {
    return false;
  }

  return true;
}

}  // namespace

template <>
struct StructTraits<mojo_base::mojom::String16DataView, TruncatedString16> {
  static void SetToNull(TruncatedString16* output) { output->string.reset(); }

  static bool Read(mojo_base::mojom::String16DataView input,
                   TruncatedString16* output) {
    if (input.is_null()) {
      output->string.reset();
      return true;
    }
    mojo::ArrayDataView<uint16_t> buffer_view;
    input.GetDataDataView(&buffer_view);
    if (buffer_view.size() > 4 * 1024)
      return false;

    output->string.emplace();
    return StructTraits<mojo_base::mojom::String16DataView,
                        base::string16>::Read(input, &output->string.value());
  }
};

bool StructTraits<blink::mojom::ManifestDataView, ::blink::Manifest>::Read(
    blink::mojom::ManifestDataView data,
    ::blink::Manifest* out) {
  TruncatedString16 string;
  if (!data.ReadName(&string))
    return false;
  out->name = std::move(string.string);

  if (!data.ReadShortName(&string))
    return false;
  out->short_name = std::move(string.string);

  if (!data.ReadDescription(&string))
    return false;
  out->description = std::move(string.string);

  if (!data.ReadGcmSenderId(&string))
    return false;
  out->gcm_sender_id = std::move(string.string);

  if (!data.ReadStartUrl(&out->start_url))
    return false;

  if (!data.ReadIcons(&out->icons))
    return false;

  if (!data.ReadScreenshots(&out->screenshots))
    return false;

  if (!data.ReadShortcuts(&out->shortcuts))
    return false;

  if (!data.ReadShareTarget(&out->share_target))
    return false;

  if (!data.ReadFileHandlers(&out->file_handlers))
    return false;

  if (!data.ReadProtocolHandlers(&out->protocol_handlers))
    return false;

  if (!data.ReadUrlHandlers(&out->url_handlers))
    return false;

  if (!data.ReadRelatedApplications(&out->related_applications))
    return false;

  out->prefer_related_applications = data.prefer_related_applications();

  if (data.has_theme_color())
    out->theme_color = data.theme_color();

  if (data.has_background_color())
    out->background_color = data.background_color();

  if (!data.ReadDisplay(&out->display))
    return false;

  if (!data.ReadDisplayOverride(&out->display_override))
    return false;

  if (!data.ReadOrientation(&out->orientation))
    return false;

  if (!data.ReadScope(&out->scope))
    return false;

  if (!data.ReadCaptureLinks(&out->capture_links))
    return false;

  return true;
}

bool StructTraits<blink::mojom::ManifestImageResourceDataView,
                  ::blink::Manifest::ImageResource>::
    Read(blink::mojom::ManifestImageResourceDataView data,
         ::blink::Manifest::ImageResource* out) {
  if (!data.ReadSrc(&out->src))
    return false;

  TruncatedString16 string;
  if (!data.ReadType(&string))
    return false;

  if (!string.string)
    return false;

  out->type = *std::move(string.string);

  if (!data.ReadSizes(&out->sizes))
    return false;

  if (!data.ReadPurpose(&out->purpose))
    return false;

  return true;
}

bool StructTraits<blink::mojom::ManifestShortcutItemDataView,
                  ::blink::Manifest::ShortcutItem>::
    Read(blink::mojom::ManifestShortcutItemDataView data,
         ::blink::Manifest::ShortcutItem* out) {
  if (!data.ReadName(&out->name))
    return false;

  TruncatedString16 string;
  if (!data.ReadShortName(&string))
    return false;
  out->short_name = std::move(string.string);

  if (!data.ReadDescription(&string))
    return false;
  out->description = std::move(string.string);

  if (!data.ReadUrl(&out->url))
    return false;

  if (!data.ReadIcons(&out->icons))
    return false;

  return true;
}

bool StructTraits<blink::mojom::ManifestRelatedApplicationDataView,
                  ::blink::Manifest::RelatedApplication>::
    Read(blink::mojom::ManifestRelatedApplicationDataView data,
         ::blink::Manifest::RelatedApplication* out) {
  TruncatedString16 string;
  if (!data.ReadPlatform(&string))
    return false;
  out->platform = std::move(string.string);

  base::Optional<GURL> url;
  if (!data.ReadUrl(&url))
    return false;
  out->url = std::move(url).value_or(GURL());

  if (!data.ReadId(&string))
    return false;
  out->id = std::move(string.string);

  return !out->url.is_empty() || out->id;
}

bool StructTraits<blink::mojom::ManifestFileFilterDataView,
                  ::blink::Manifest::FileFilter>::
    Read(blink::mojom::ManifestFileFilterDataView data,
         ::blink::Manifest::FileFilter* out) {
  TruncatedString16 name;
  if (!data.ReadName(&name))
    return false;

  if (!name.string)
    return false;

  out->name = *std::move(name.string);

  if (!data.ReadAccept(&out->accept))
    return false;

  return true;
}

bool StructTraits<blink::mojom::ManifestUrlHandlerDataView,
                  ::blink::Manifest::UrlHandler>::
    Read(blink::mojom::ManifestUrlHandlerDataView data,
         ::blink::Manifest::UrlHandler* out) {
  if (!data.ReadOrigin(&out->origin))
    return false;

  // Make sure the origin is valid.
  if (!IsHostValidForUrlHandler(out->origin.host()))
    return false;

  out->has_origin_wildcard = data.has_origin_wildcard();

  return true;
}

bool StructTraits<blink::mojom::ManifestShareTargetParamsDataView,
                  ::blink::Manifest::ShareTargetParams>::
    Read(blink::mojom::ManifestShareTargetParamsDataView data,
         ::blink::Manifest::ShareTargetParams* out) {
  TruncatedString16 string;
  if (!data.ReadText(&string))
    return false;
  out->text = std::move(string.string);

  if (!data.ReadTitle(&string))
    return false;
  out->title = std::move(string.string);

  if (!data.ReadUrl(&string))
    return false;
  out->url = std::move(string.string);

  if (!data.ReadFiles(&out->files))
    return false;

  return true;
}

bool StructTraits<blink::mojom::ManifestShareTargetDataView,
                  ::blink::Manifest::ShareTarget>::
    Read(blink::mojom::ManifestShareTargetDataView data,
         ::blink::Manifest::ShareTarget* out) {
  if (!data.ReadAction(&out->action))
    return false;

  if (!data.ReadMethod(&out->method))
    return false;

  if (!data.ReadEnctype(&out->enctype))
    return false;

  return data.ReadParams(&out->params);
}

bool StructTraits<blink::mojom::ManifestFileHandlerDataView,
                  ::blink::Manifest::FileHandler>::
    Read(blink::mojom::ManifestFileHandlerDataView data,
         ::blink::Manifest::FileHandler* out) {
  if (!data.ReadAction(&out->action))
    return false;

  if (!data.ReadName(&out->name))
    return false;

  if (!data.ReadAccept(&out->accept))
    return false;

  return true;
}

bool StructTraits<blink::mojom::ManifestProtocolHandlerDataView,
                  ::blink::Manifest::ProtocolHandler>::
    Read(blink::mojom::ManifestProtocolHandlerDataView data,
         ::blink::Manifest::ProtocolHandler* out) {
  if (!data.ReadProtocol(&out->protocol))
    return false;

  if (!data.ReadUrl(&out->url))
    return false;

  return true;
}

}  // namespace mojo
