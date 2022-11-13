// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/static_data_navigation_body_loader.h"

namespace blink {

StaticDataNavigationBodyLoader::StaticDataNavigationBodyLoader() {}

StaticDataNavigationBodyLoader::~StaticDataNavigationBodyLoader() = default;

void StaticDataNavigationBodyLoader::Write(const char* data, size_t size) {
  DCHECK(!received_all_data_);
  if (!data_)
    data_ = SharedBuffer::Create(data, size);
  else
    data_->Append(data, size);
  Continue();
}

void StaticDataNavigationBodyLoader::Write(const SharedBuffer& data) {
  DCHECK(!received_all_data_);
  if (!data_)
    data_ = SharedBuffer::Create();
  data_->Append(data);
  Continue();
}

void StaticDataNavigationBodyLoader::Finish() {
  DCHECK(!received_all_data_);
  received_all_data_ = true;
  Continue();
}

void StaticDataNavigationBodyLoader::SetDefersLoading(LoaderFreezeMode mode) {
  freeze_mode_ = mode;
  Continue();
}

void StaticDataNavigationBodyLoader::StartLoadingBody(
    WebNavigationBodyLoader::Client* client) {
  DCHECK(!is_in_continue_);
  client_ = client;
  Continue();
}

void StaticDataNavigationBodyLoader::Continue() {
  if (freeze_mode_ != LoaderFreezeMode::kNone || !client_ || is_in_continue_)
    return;

  // We don't want reentrancy in this method -
  // protect with a boolean. Cannot use AutoReset
  // because |this| can be deleted before reset.
  is_in_continue_ = true;
  base::WeakPtr<StaticDataNavigationBodyLoader> weak_self =
      weak_factory_.GetWeakPtr();

  if (!sent_all_data_) {
    while (data_ && data_->size()) {
      total_encoded_data_length_ += data_->size();

      // Cleanup |data_| before dispatching, so that
      // we can reentrantly append some data again.
      scoped_refptr<SharedBuffer> data = std::move(data_);

      for (const auto& span : *data) {
        client_->BodyDataReceived(span);
        // |this| can be destroyed from BodyDataReceived.
        if (!weak_self)
          return;
      }

      if (freeze_mode_ != LoaderFreezeMode::kNone) {
        is_in_continue_ = false;
        return;
      }
    }
    if (received_all_data_)
      sent_all_data_ = true;
  }

  if (sent_all_data_) {
    // Clear |client_| to avoid any extra notifications from reentrancy.
    WebNavigationBodyLoader::Client* client = client_;
    client_ = nullptr;
    client->BodyLoadingFinished(
        base::TimeTicks::Now(), total_encoded_data_length_,
        total_encoded_data_length_, total_encoded_data_length_, false,
        absl::nullopt);
    // |this| can be destroyed from BodyLoadingFinished.
    if (!weak_self)
      return;
  }

  is_in_continue_ = false;
}

}  // namespace blink
