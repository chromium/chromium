// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_event_holder.h"

ChooseFileEventHolder::ChooseFileEventHolder() = default;

ChooseFileEventHolder::~ChooseFileEventHolder() = default;

ChooseFileEventHolder* ChooseFileEventHolder::GetInstance() {
  static base::NoDestructor<ChooseFileEventHolder> instance;
  return instance.get();
}

std::optional<ChooseFileEvent>
ChooseFileEventHolder::ResetLastChooseFileEvent() {
  return std::exchange(last_choose_file_event_, std::nullopt);
}

void ChooseFileEventHolder::SetLastChooseFileEvent(ChooseFileEvent event) {
  last_choose_file_event_ = std::move(event);
}

bool ChooseFileEventHolder::HasLastChooseFileEvent() const {
  return last_choose_file_event_.has_value();
}
