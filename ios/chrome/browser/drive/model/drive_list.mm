// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_list.h"

#import "base/notreached.h"

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
  background_image_link = [other.background_image_link copy];
  created_time = other.created_time;
  modified_time = other.modified_time;
  modified_by_me_time = other.modified_by_me_time;
  viewed_by_me_time = other.viewed_by_me_time;
  shared_with_me_time = other.shared_with_me_time;
  parent_identifier = [other.parent_identifier copy];
  is_shared_drive = other.is_shared_drive;
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
  std::swap(background_image_link, other.background_image_link);
  std::swap(created_time, other.created_time);
  std::swap(modified_time, other.modified_time);
  std::swap(modified_by_me_time, other.modified_by_me_time);
  std::swap(viewed_by_me_time, other.viewed_by_me_time);
  std::swap(shared_with_me_time, other.shared_with_me_time);
  std::swap(parent_identifier, other.parent_identifier);
  std::swap(is_shared_drive, other.is_shared_drive);
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

void DriveList::ListItems(const DriveListQuery& query,
                          DriveListCompletionCallback completion_callback) {
  NOTREACHED();
}

void DriveList::ListFiles(const DriveListQuery& query,
                          DriveListCompletionCallback completion_callback) {
  ListItems(query, std::move(completion_callback));
}

void DriveList::ListSharedDrives(
    const DriveListQuery& query,
    DriveListCompletionCallback completion_callback) {
  NOTREACHED();
}
