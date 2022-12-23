// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/smart_card_delegate.h"

namespace content {

SmartCardDelegate::SmartCardDelegate() = default;
SmartCardDelegate::~SmartCardDelegate() = default;

void SmartCardDelegate::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SmartCardDelegate::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace content
