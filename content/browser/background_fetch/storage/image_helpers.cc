// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/image_helpers.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "ui/gfx/image/image.h"

namespace content {
namespace background_fetch {

namespace {

// The max icon resolution, this is used as a threshold to decide
// whether the icon should be persisted.
constexpr int kMaxIconResolution = 256 * 256;

std::string ConvertAndSerializeIcon(const SkBitmap& icon) {
  std::string serialized_icon;
  auto icon_bytes = gfx::Image::CreateFrom1xBitmap(icon).As1xPNGBytes();
  serialized_icon.assign(icon_bytes->front_as<char>(),
                         icon_bytes->front_as<char>() + icon_bytes->size());
  return serialized_icon;
}

SkBitmap DeserializeAndConvertIcon(
    std::unique_ptr<std::string> serialized_icon) {
  return gfx::Image::CreateFrom1xPNGBytes(
             reinterpret_cast<const unsigned char*>(serialized_icon->c_str()),
             serialized_icon->size())
      .AsBitmap();
}

}  // namespace

bool ShouldPersistIcon(const SkBitmap& icon) {
  return !icon.isNull() && (icon.height() * icon.width() <= kMaxIconResolution);
}

void SerializeIcon(const SkBitmap& icon, SerializeIconCallback callback) {
  DCHECK(!icon.isNull());
  // Do the serialization on a seperate thread to avoid blocking on
  // expensive operations (image conversions), then post back to current
  // thread and continue normally.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ConvertAndSerializeIcon, icon), std::move(callback));
}

void DeserializeIcon(std::unique_ptr<std::string> serialized_icon,
                     DeserializeIconCallback callback) {
  DCHECK(serialized_icon);
  // Do the deserialization on a seperate thread to avoid blocking on
  // expensive operations (image conversions), then post back to current
  // thread and continue normally.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeserializeAndConvertIcon, std::move(serialized_icon)),
      base::BindOnce(std::move(callback)));
}

}  // namespace background_fetch
}  // namespace content
