// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/user_agent/user_agent_mojom_traits.h"

#include <string>

namespace mojo {

bool StructTraits<blink::mojom::UserAgentBrandVersionDataView,
                  ::blink::UserAgentBrandVersion>::
    Read(blink::mojom::UserAgentBrandVersionDataView data,
         ::blink::UserAgentBrandVersion* out) {
  if (!data.ReadBrand(&out->brand))
    return false;

  if (!data.ReadVersion(&out->version))
    return false;

  return true;
}

bool StructTraits<blink::mojom::UserAgentMetadataDataView,
                  ::blink::UserAgentMetadata>::
    Read(blink::mojom::UserAgentMetadataDataView data,
         ::blink::UserAgentMetadata* out) {
  std::string string;
  blink::UserAgentBrandList user_agent_brand_list;
  blink::UserAgentBrandList user_agent_brand_full_version_list;

  if (!data.ReadBrandVersionList(&user_agent_brand_list))
    return false;
  out->brand_version_list = std::move(user_agent_brand_list);

  if (!data.ReadBrandFullVersionList(&user_agent_brand_full_version_list))
    return false;
  out->brand_full_version_list = std::move(user_agent_brand_full_version_list);

  if (!data.ReadFullVersion(&string))
    return false;
  out->full_version = string;

  if (!data.ReadPlatform(&string))
    return false;
  out->platform = string;

  if (!data.ReadPlatformVersion(&string))
    return false;
  out->platform_version = string;

  if (!data.ReadArchitecture(&string))
    return false;
  out->architecture = string;

  if (!data.ReadModel(&string))
    return false;
  out->model = string;
  out->mobile = data.mobile();

  if (!data.ReadBitness(&string))
    return false;
  out->bitness = string;
  out->wow64 = data.wow64();

  if (!data.ReadFormFactors(&out->form_factors)) {
    return false;
  }

  return true;
}

bool StructTraits<blink::mojom::UserAgentOverrideDataView,
                  ::blink::UserAgentOverride>::
    Read(blink::mojom::UserAgentOverrideDataView data,
         ::blink::UserAgentOverride* out) {
  if (!data.ReadUaStringOverride(&out->ua_string_override) ||
      !data.ReadUaMetadataOverride(&out->ua_metadata_override)) {
    return false;
  }
  return true;
}

}  // namespace mojo
