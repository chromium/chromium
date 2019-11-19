// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_index/content_index_icon_loader.h"

#include "base/barrier_closure.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_icon_sizes_parser.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/threaded_icon_loader.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"

namespace blink {

namespace {

constexpr base::TimeDelta kIconFetchTimeout = base::TimeDelta::FromSeconds(30);

void FetchIcon(ExecutionContext* execution_context,
               const KURL& icon_url,
               const WebSize& icon_size,
               ThreadedIconLoader::IconCallback callback) {
  ResourceRequest resource_request(icon_url);
  resource_request.SetRequestContext(mojom::RequestContextType::IMAGE);
  resource_request.SetPriority(ResourceLoadPriority::kMedium);
  resource_request.SetTimeoutInterval(kIconFetchTimeout);

  auto* threaded_icon_loader = MakeGarbageCollected<ThreadedIconLoader>();
  threaded_icon_loader->Start(execution_context, resource_request, icon_size,
                              std::move(callback));
}

WebVector<Manifest::ImageResource> ToImageResource(
    ExecutionContext* execution_context,
    const Vector<mojom::blink::ContentIconDefinitionPtr>& icon_definitions) {
  WebVector<Manifest::ImageResource> image_resources;
  for (const auto& icon_definition : icon_definitions) {
    Manifest::ImageResource image_resource;
    image_resource.src = execution_context->CompleteURL(icon_definition->src);
    image_resource.type = WebString(icon_definition->type).Utf16();
    for (const auto& size :
         WebIconSizesParser::ParseIconSizes(icon_definition->sizes)) {
      image_resource.sizes.emplace_back(size.width, size.height);
    }
    if (image_resource.sizes.empty())
      image_resource.sizes.emplace_back(0, 0);
    image_resource.purpose.push_back(Manifest::ImageResource::Purpose::ANY);
    image_resources.emplace_back(std::move(image_resource));
  }
  return image_resources;
}

KURL FindBestIcon(WebVector<Manifest::ImageResource> image_resources,
                  const WebSize& icon_size) {
  return KURL(ManifestIconSelector::FindBestMatchingIcon(
      image_resources.ReleaseVector(),
      /* ideal_icon_height_in_px= */ icon_size.height,
      /* minimum_icon_size_in_px= */ 0,
      /* max_width_to_height_ratio= */ icon_size.width * 1.0f /
          icon_size.height,
      Manifest::ImageResource::Purpose::ANY));
}

}  // namespace

ContentIndexIconLoader::ContentIndexIconLoader() = default;

void ContentIndexIconLoader::Start(
    ExecutionContext* execution_context,
    mojom::blink::ContentDescriptionPtr description,
    const Vector<WebSize>& icon_sizes,
    IconsCallback callback) {
  DCHECK(!description->icons.IsEmpty());
  DCHECK(!icon_sizes.IsEmpty());

  auto image_resources = ToImageResource(execution_context, description->icons);

  auto icons = std::make_unique<Vector<SkBitmap>>();
  icons->ReserveCapacity(icon_sizes.size());
  Vector<SkBitmap>* icons_ptr = icons.get();
  auto barrier_closure = base::BarrierClosure(
      icon_sizes.size(),
      WTF::Bind(&ContentIndexIconLoader::DidGetIcons, WrapPersistent(this),
                std::move(description), std::move(icons), std::move(callback)));

  for (const auto& icon_size : icon_sizes) {
    // TODO(crbug.com/973844): The same `src` may be chosen more than once.
    // This should probably only be downloaded once and resized.
    KURL icon_url = FindBestIcon(image_resources, icon_size);

    if (icon_url.IsEmpty())
      icon_url = KURL(image_resources[0].src);

    // |icons_ptr| is safe to use since it is owned by |barrier_closure|.
    FetchIcon(
        execution_context, icon_url, icon_size,
        WTF::Bind(
            [](base::OnceClosure done_closure, Vector<SkBitmap>* icons_ptr,
               SkBitmap icon, double resize_scale) {
              icons_ptr->push_back(std::move(icon));
              std::move(done_closure).Run();
            },
            barrier_closure, WTF::Unretained(icons_ptr)));
  }
}

void ContentIndexIconLoader::DidGetIcons(
    mojom::blink::ContentDescriptionPtr description,
    std::unique_ptr<Vector<SkBitmap>> icons,
    IconsCallback callback) {
  DCHECK(icons);
  std::move(callback).Run(std::move(description), std::move(*icons));
}

}  // namespace blink
