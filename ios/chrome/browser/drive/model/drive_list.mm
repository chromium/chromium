// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_list.h"

#pragma mark - DriveItem

DriveItem::DriveItem() = default;

DriveItem::DriveItem(const DriveItem& other) : DriveItem() {
  *this = other;
}

DriveItem::DriveItem(DriveItem&& other) : DriveItem() {
  *this = std::move(other);
}

DriveItem::~DriveItem() = default;

DriveItem& DriveItem::operator=(const DriveItem& other) {
  identifier = [other.identifier copy];
  name = [other.name copy];
  icon_link = [other.icon_link copy];
  thumbnail_link = [other.thumbnail_link copy];
  modified_time = other.modified_time;
  parent_identifier = [other.parent_identifier copy];
  is_folder = other.is_folder;
  mime_type = [other.mime_type copy];
  size = other.size;
  can_download = other.can_download;
  return *this;
}

DriveItem& DriveItem::operator=(DriveItem&& other) {
  std::swap(identifier, other.identifier);
  std::swap(name, other.name);
  std::swap(icon_link, other.icon_link);
  std::swap(thumbnail_link, other.thumbnail_link);
  std::swap(modified_time, other.modified_time);
  std::swap(parent_identifier, other.parent_identifier);
  std::swap(is_folder, other.is_folder);
  std::swap(mime_type, other.mime_type);
  std::swap(size, other.size);
  std::swap(can_download, other.can_download);
  return *this;
}

#pragma mark - DriveListResult

DriveListResult::DriveListResult() = default;

DriveListResult::DriveListResult(const DriveListResult& other)
    : DriveListResult() {
  *this = other;
}

DriveListResult::DriveListResult(DriveListResult&& other) : DriveListResult() {
  *this = std::move(other);
}

DriveListResult::~DriveListResult() = default;

DriveListResult& DriveListResult::operator=(const DriveListResult& other) {
  items = other.items;
  next_page_token = [other.next_page_token copy];
  error = [other.error copy];
  return *this;
}

DriveListResult& DriveListResult::operator=(DriveListResult&& other) {
  std::swap(items, other.items);
  std::swap(next_page_token, other.next_page_token);
  std::swap(error, other.error);
  return *this;
}

#pragma mark - DriveList

DriveList::DriveList() = default;

DriveList::~DriveList() = default;
