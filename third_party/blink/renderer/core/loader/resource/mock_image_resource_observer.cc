// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/mock_image_resource_observer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"

namespace blink {

MockImageResourceObserver::MockImageResourceObserver(
    ImageResourceContent* content)
    : content_(content),
      image_changed_count_(0),
      image_width_on_last_image_changed_(0),
      image_notify_finished_count_(0),
      image_width_on_image_notify_finished_(0) {
  content_->AddObserver(this);
}

void MockImageResourceObserver::RemoveAsObserver() {
  if (!content_)
    return;
  content_->RemoveObserver(this);
  content_ = nullptr;
}

void MockImageResourceObserver::ImageChanged(ImageResourceContent* image,
                                             CanDeferInvalidation defer) {
  image_changed_count_++;
  image_width_on_last_image_changed_ =
      content_->HasImage() ? content_->GetImage()->width() : 0;
  defer_ = defer;
}

void MockImageResourceObserver::ImageNotifyFinished(
    ImageResourceContent* image) {
  ASSERT_EQ(0, image_notify_finished_count_);
  DCHECK(image->IsLoaded());
  image_notify_finished_count_++;
  image_width_on_image_notify_finished_ =
      content_->HasImage() ? content_->GetImage()->width() : 0;
  status_on_image_notify_finished_ = content_->GetContentStatus();
}

bool MockImageResourceObserver::ImageNotifyFinishedCalled() const {
  DCHECK_LE(image_notify_finished_count_, 1);
  return image_notify_finished_count_;
}

void MockImageResourceObserver::Trace(Visitor* visitor) const {
  visitor->Trace(content_);
  ImageResourceObserver::Trace(visitor);
}

}  // namespace blink
