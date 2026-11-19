// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_ui_data.h"

namespace content {

NavigationUIData::~NavigationUIData() = default;

std::unique_ptr<NavigationUIData>
NavigationUIData::MaybeCreateForRedispatchedNavigation() {
  return nullptr;
}

void NavigationUIData::PopulateFromNavigationHandle(
    NavigationHandle& navigation_handle) {}

CloneableNavigationUIDataPtr::CloneableNavigationUIDataPtr() = default;

CloneableNavigationUIDataPtr::CloneableNavigationUIDataPtr(std::nullptr_t) {}

CloneableNavigationUIDataPtr::CloneableNavigationUIDataPtr(
    std::unique_ptr<NavigationUIData> ptr)
    : ptr_(std::move(ptr)) {}

CloneableNavigationUIDataPtr::~CloneableNavigationUIDataPtr() = default;

CloneableNavigationUIDataPtr::CloneableNavigationUIDataPtr(
    const CloneableNavigationUIDataPtr& other)
    : ptr_(other.Clone()) {}

CloneableNavigationUIDataPtr& CloneableNavigationUIDataPtr::operator=(
    const CloneableNavigationUIDataPtr& other) {
  if (this != &other) {
    ptr_ = other.Clone();
  }

  return *this;
}

CloneableNavigationUIDataPtr::CloneableNavigationUIDataPtr(
    CloneableNavigationUIDataPtr&& other) noexcept = default;

CloneableNavigationUIDataPtr& CloneableNavigationUIDataPtr::operator=(
    CloneableNavigationUIDataPtr&& other) noexcept = default;

}  // namespace content
