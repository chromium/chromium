// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/notifications/notification_resources_loader.h"

#include <cmath>
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

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
  LoadImage(context, NotificationImageLoader::Type::kImage,
            notification_data.image,
            WTF::Bind(&NotificationResourcesLoader::DidLoadImage,
                      WrapWeakPersistent(this)));
  LoadImage(context, NotificationImageLoader::Type::kIcon,
            notification_data.icon,
            WTF::Bind(&NotificationResourcesLoader::DidLoadIcon,
                      WrapWeakPersistent(this)));
  LoadImage(context, NotificationImageLoader::Type::kBadge,
            notification_data.badge,
            WTF::Bind(&NotificationResourcesLoader::DidLoadBadge,
                      WrapWeakPersistent(this)));

  action_icons_.resize(num_actions);
  for (wtf_size_t i = 0; i < num_actions; i++)
    LoadImage(context, NotificationImageLoader::Type::kActionIcon,
              notification_data.actions.value()[i]->icon,
              WTF::Bind(&NotificationResourcesLoader::DidLoadActionIcon,
                        WrapWeakPersistent(this), i));
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
  for (auto image_loader : image_loaders_)
    image_loader->Stop();
}

void NotificationResourcesLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(image_loaders_);
}

void NotificationResourcesLoader::LoadImage(
    ExecutionContext* context,
    NotificationImageLoader::Type type,
    const KURL& url,
    NotificationImageLoader::ImageCallback image_callback) {
  if (url.IsNull() || url.IsEmpty() || !url.IsValid()) {
    DidFinishRequest();
    return;
  }

  NotificationImageLoader* image_loader = new NotificationImageLoader(type);
  image_loaders_.push_back(image_loader);
  image_loader->Start(context, url, std::move(image_callback));
}

void NotificationResourcesLoader::DidLoadImage(const SkBitmap& image) {
  image_ = NotificationImageLoader::ScaleDownIfNeeded(
      image, NotificationImageLoader::Type::kImage);
  DidFinishRequest();
}

void NotificationResourcesLoader::DidLoadIcon(const SkBitmap& image) {
  icon_ = NotificationImageLoader::ScaleDownIfNeeded(
      image, NotificationImageLoader::Type::kIcon);
  DidFinishRequest();
}

void NotificationResourcesLoader::DidLoadBadge(const SkBitmap& image) {
  badge_ = NotificationImageLoader::ScaleDownIfNeeded(
      image, NotificationImageLoader::Type::kBadge);
  DidFinishRequest();
}

void NotificationResourcesLoader::DidLoadActionIcon(wtf_size_t action_index,
                                                    const SkBitmap& image) {
  DCHECK_LT(action_index, action_icons_.size());

  action_icons_[action_index] = NotificationImageLoader::ScaleDownIfNeeded(
      image, NotificationImageLoader::Type::kActionIcon);
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
