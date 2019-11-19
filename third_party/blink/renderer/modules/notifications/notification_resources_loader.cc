// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/notifications/notification_resources_loader.h"

#include <cmath>

#include "base/optional.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom-blink.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

namespace {

// 99.9% of all images were fetched successfully in 90 seconds.
constexpr base::TimeDelta kImageFetchTimeout = base::TimeDelta::FromSeconds(90);

enum class NotificationIconType { kImage, kIcon, kBadge, kActionIcon };

WebSize GetIconDimensions(NotificationIconType type) {
  switch (type) {
    case NotificationIconType::kImage:
      return {kNotificationMaxImageWidthPx, kNotificationMaxImageHeightPx};
    case NotificationIconType::kIcon:
      return {kNotificationMaxIconSizePx, kNotificationMaxIconSizePx};
    case NotificationIconType::kBadge:
      return {kNotificationMaxBadgeSizePx, kNotificationMaxBadgeSizePx};
    case NotificationIconType::kActionIcon:
      return {kNotificationMaxActionIconSizePx,
              kNotificationMaxActionIconSizePx};
  }
}

}  // namespace

NotificationResourcesLoader::NotificationResourcesLoader(
    CompletionCallback completion_callback)
    : started_(false),
      completion_callback_(std::move(completion_callback)),
      pending_request_count_(0) {
  DCHECK(completion_callback_);
}

NotificationResourcesLoader::~NotificationResourcesLoader() = default;

void NotificationResourcesLoader::Start(
    ExecutionContext* context,
    const mojom::blink::NotificationData& notification_data) {
  DCHECK(!started_);
  started_ = true;

  wtf_size_t num_actions = notification_data.actions.has_value()
                               ? notification_data.actions->size()
                               : 0;
  pending_request_count_ = 3 /* image, icon, badge */ + num_actions;

  // TODO(johnme): ensure image is not loaded when it will not be used.
  // TODO(mvanouwerkerk): ensure no badge is loaded when it will not be used.
  LoadIcon(context, notification_data.image,
           GetIconDimensions(NotificationIconType::kImage),
           WTF::Bind(&NotificationResourcesLoader::DidLoadIcon,
                     WrapWeakPersistent(this), WTF::Unretained(&image_)));
  LoadIcon(context, notification_data.icon,
           GetIconDimensions(NotificationIconType::kIcon),
           WTF::Bind(&NotificationResourcesLoader::DidLoadIcon,
                     WrapWeakPersistent(this), WTF::Unretained(&icon_)));
  LoadIcon(context, notification_data.badge,
           GetIconDimensions(NotificationIconType::kBadge),
           WTF::Bind(&NotificationResourcesLoader::DidLoadIcon,
                     WrapWeakPersistent(this), WTF::Unretained(&badge_)));

  action_icons_.Grow(num_actions);
  for (wtf_size_t i = 0; i < num_actions; i++) {
    LoadIcon(context, notification_data.actions.value()[i]->icon,
             GetIconDimensions(NotificationIconType::kActionIcon),
             WTF::Bind(&NotificationResourcesLoader::DidLoadIcon,
                       WrapWeakPersistent(this),
                       WTF::Unretained(&action_icons_[i])));
  }
}

mojom::blink::NotificationResourcesPtr
NotificationResourcesLoader::GetResources() const {
  auto resources = mojom::blink::NotificationResources::New();
  resources->image = image_;
  resources->icon = icon_;
  resources->badge = badge_;
  resources->action_icons = action_icons_;
  return resources;
}

void NotificationResourcesLoader::Stop() {
  for (const auto& icon_loader : icon_loaders_)
    icon_loader->Stop();
}

void NotificationResourcesLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(icon_loaders_);
}

void NotificationResourcesLoader::LoadIcon(
    ExecutionContext* context,
    const KURL& url,
    const WebSize& resize_dimensions,
    ThreadedIconLoader::IconCallback icon_callback) {
  if (url.IsNull() || url.IsEmpty() || !url.IsValid()) {
    std::move(icon_callback).Run(SkBitmap(), -1.0);
    return;
  }

  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(mojom::RequestContextType::IMAGE);
  resource_request.SetPriority(ResourceLoadPriority::kMedium);
  resource_request.SetTimeoutInterval(kImageFetchTimeout);

  auto* icon_loader = MakeGarbageCollected<ThreadedIconLoader>();
  icon_loaders_.push_back(icon_loader);
  icon_loader->Start(context, resource_request, resize_dimensions,
                     std::move(icon_callback));
}

void NotificationResourcesLoader::DidLoadIcon(SkBitmap* out_icon,
                                              SkBitmap icon,
                                              double resize_scale) {
  *out_icon = std::move(icon);
  DidFinishRequest();
}

void NotificationResourcesLoader::DidFinishRequest() {
  DCHECK_GT(pending_request_count_, 0);
  pending_request_count_--;
  if (!pending_request_count_) {
    Stop();
    std::move(completion_callback_).Run(this);
    // The |this| pointer may have been deleted now.
  }
}

}  // namespace blink
