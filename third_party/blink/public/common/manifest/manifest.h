// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom-shared.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace blink {

// The Manifest structure is an internal representation of the Manifest file
// described in the "Manifest for Web Application" document:
// http://w3c.github.io/manifest/
struct BLINK_COMMON_EXPORT Manifest {
  // Structure representing an icon as per the Manifest specification, see:
  // https://w3c.github.io/manifest/#dom-imageresource
  struct BLINK_COMMON_EXPORT ImageResource {
    enum class Purpose {
      ANY = 0,
      MONOCHROME,
      MASKABLE,
      IMAGE_RESOURCE_PURPOSE_LAST = MASKABLE,
    };

    ImageResource();
    ImageResource(const ImageResource& other);
    ~ImageResource();

    bool operator==(const ImageResource& other) const;

    // MUST be a valid url. If an icon doesn't have a valid URL, it will not be
    // successfully parsed, thus will not be represented in the Manifest.
    GURL src;

    // Empty if the parsing failed or the field was not present. The type can be
    // any string and doesn't have to be a valid image MIME type at this point.
    // It is up to the consumer of the object to check if the type matches a
    // supported type.
    base::string16 type;

    // Empty if the parsing failed, the field was not present or empty.
    // The special value "any" is represented by gfx::Size(0, 0).
    std::vector<gfx::Size> sizes;

    // Never empty. Defaults to a vector with a single value, IconPurpose::ANY,
    // if not explicitly specified in the manifest.
    std::vector<Purpose> purpose;
  };

  // Structure representing a shortcut as per the Manifest specification, see:
  // https://w3c.github.io/manifest/#shortcuts-member
  struct BLINK_COMMON_EXPORT ShortcutItem {
    ShortcutItem();
    ~ShortcutItem();

    base::string16 name;
    base::Optional<base::string16> short_name;
    base::Optional<base::string16> description;
    GURL url;
    std::vector<ImageResource> icons;
  };

  struct BLINK_COMMON_EXPORT FileFilter {
    base::string16 name;
    std::vector<base::string16> accept;
  };

  // Structure representing a Web Share target's query parameter keys.
  struct BLINK_COMMON_EXPORT ShareTargetParams {
    ShareTargetParams();
    ~ShareTargetParams();

    base::Optional<base::string16> title;
    base::Optional<base::string16> text;
    base::Optional<base::string16> url;
    std::vector<FileFilter> files;
  };

  // Structure representing how a Web Share target handles an incoming share.
  struct BLINK_COMMON_EXPORT ShareTarget {
    enum class Method {
      kGet,
      kPost,
    };

    enum class Enctype {
      kFormUrlEncoded,
      kMultipartFormData,
    };

    ShareTarget();
    ~ShareTarget();

    // The URL used for sharing. Query parameters are added to this comprised of
    // keys from |params| and values from the shared data.
    GURL action;

    // The HTTP request method for the web share target.
    Method method;

    // The way that share data is encoded in "POST" request.
    Enctype enctype;

    ShareTargetParams params;
  };

  // Structure representing a File Handler.
  struct BLINK_COMMON_EXPORT FileHandler {
    // The URL which will be opened when the file handler is invoked.
    GURL action;
    base::string16 name;
    std::map<base::string16, std::vector<base::string16>> accept;
  };

  // Structure representing a Protocol Handler.
  struct BLINK_COMMON_EXPORT ProtocolHandler {
    base::string16 protocol;
    GURL url;
  };

  // Structure representing a related application.
  struct BLINK_COMMON_EXPORT RelatedApplication {
    RelatedApplication();
    ~RelatedApplication();

    // The platform on which the application can be found. This can be any
    // string, and is interpreted by the consumer of the object. Empty if the
    // parsing failed.
    base::Optional<base::string16> platform;

    // URL at which the application can be found. One of |url| and |id| must be
    // present. Empty if the parsing failed or the field was not present.
    GURL url;

    // An id which is used to represent the application on the platform. One of
    // |url| and |id| must be present. Empty if the parsing failed or the field
    // was not present.
    base::Optional<base::string16> id;
  };

  Manifest();
  Manifest(const Manifest& other);
  ~Manifest();

  // Returns whether this Manifest had no attribute set. A newly created
  // Manifest is always empty.
  bool IsEmpty() const;

  // Null if the parsing failed or the field was not present.
  base::Optional<base::string16> name;

  // Null if the parsing failed or the field was not present.
  base::Optional<base::string16> short_name;

  // Empty if the parsing failed or the field was not present.
  GURL start_url;

  // Set to DisplayMode::kUndefined if the parsing failed or the field was not
  // present.
  blink::mojom::DisplayMode display = blink::mojom::DisplayMode::kUndefined;

  // Empty if the parsing failed, the field was not present, or all the
  // values inside the JSON array were invalid.
  std::vector<blink::mojom::DisplayMode> display_override;

  // Set to device::mojom::ScreenOrientationLockType::DEFAULT if the parsing
  // failed or the field was not present.
  device::mojom::ScreenOrientationLockType orientation =
      device::mojom::ScreenOrientationLockType::DEFAULT;

  // Empty if the parsing failed, the field was not present, or all the
  // icons inside the JSON array were invalid.
  std::vector<ImageResource> icons;

  // Empty if the parsing failed, the field was not present, or all the
  // icons inside the JSON array were invalid.
  std::vector<ShortcutItem> shortcuts;

  // Null if parsing failed or the field was not present.
  base::Optional<ShareTarget> share_target;

  // Empty if parsing failed or the field was not present.
  // TODO(crbug.com/829689): This field is non-standard and part of a Chrome
  // experiment. See:
  // https://github.com/WICG/file-handling/blob/master/explainer.md
  std::vector<FileHandler> file_handlers;

  // Empty if parsing failed or the field was not present.
  // TODO(crbug.com/1019239): This is going into the mainline manifest spec,
  // remove the TODO once that PR goes in.
  // The URLProtocolHandler explainer can be found here:
  // https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/master/URLProtocolHandler/explainer.md
  std::vector<ProtocolHandler> protocol_handlers;

  // Empty if the parsing failed, the field was not present, empty or all the
  // applications inside the array were invalid. The order of the array
  // indicates the priority of the application to use.
  std::vector<RelatedApplication> related_applications;

  // A boolean that is used as a hint for the user agent to say that related
  // applications should be preferred over the web application. False if missing
  // or there is a parsing failure.
  bool prefer_related_applications = false;

  // Null if field is not present or parsing failed.
  base::Optional<SkColor> theme_color;

  // Null if field is not present or parsing failed.
  base::Optional<SkColor> background_color;

  // This is a proprietary extension of the web Manifest, double-check that it
  // is okay to use this entry.
  // Null if parsing failed or the field was not present.
  base::Optional<base::string16> gcm_sender_id;

  // Empty if the parsing failed. Otherwise defaults to the start URL (or
  // document URL if start URL isn't present) with filename, query, and fragment
  // removed.
  GURL scope;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_H_
