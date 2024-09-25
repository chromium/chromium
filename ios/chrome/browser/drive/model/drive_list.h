// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_LIST_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_LIST_H_

#import <Foundation/Foundation.h>

#import <vector>

#import "base/functional/callback.h"

@protocol SystemIdentity;

// A Drive item (file, folder or shared drive).
struct DriveItem {
  DriveItem();
  DriveItem(const DriveItem& other);
  DriveItem(DriveItem&& other);
  ~DriveItem();
  DriveItem& operator=(const DriveItem& other);
  DriveItem& operator=(DriveItem&& other);

  // Unique identifier for this item.
  NSString* identifier = nil;
  // The name of this item.
  NSString* name = nil;
  // Link to the item's icon.
  NSString* icon_link = nil;
  // Link to the item's thumbnail, if available.
  NSString* thumbnail_link = nil;
  // Link to this shared drive's background image. Only populated for shared
  // drives.
  NSString* background_image_link = nil;
  // The time the item was created.
  NSDate* created_time = nil;
  // The last time the item was modified by anyone.
  NSDate* modified_time = nil;
  // The last time the item was modified by the user.
  NSDate* modified_by_me_time = nil;
  // The last time the item was viewed by the user.
  NSDate* viewed_by_me_time = nil;
  // The time the item was shared with the current user.
  NSDate* shared_with_me_time = nil;
  // Identifier of the item's parent folder.
  NSString* parent_identifier = nil;
  // Whether the item is a shared drive.
  bool is_shared_drive = false;
  // Whether the item is a folder.
  bool is_folder = false;
  // If this item is a file, the MIME type of the file.
  NSString* mime_type = nil;
  // Size in bytes. Will be 0 for folders or files that have no size, like
  // shortcuts.
  int64_t size = 0;
  // If this item is a file, whether the user can download this file directly.
  // If this is a file which cannot be downloaded directly, then it can only be
  // exported to a different MIME type.
  bool can_download = false;
};

// Results reported by the completion block of a query to list/search for files.
struct DriveListResult {
  DriveListResult();
  DriveListResult(const DriveListResult& other);
  DriveListResult(DriveListResult&& other);
  ~DriveListResult();
  DriveListResult& operator=(const DriveListResult& other);
  DriveListResult& operator=(DriveListResult&& other);

  // List of items, if list/search succeeded.
  std::vector<DriveItem> items;
  // If there are more items to list/search, this token will be populated and
  // can be used to continue the list/search.
  NSString* next_page_token = nil;
  // Error object, if list/search failed. Empty results are not treated as
  // errors.
  NSError* error = nil;
};

// Query object defining filtering/sorting criteria to search Drive items.
struct DriveListQuery {
  // Lists/searches Drive for items in folder with `folder_identifier`.
  NSString* folder_identifier = nil;
  // If not nil, only files including `contains` are returned.
  NSString* contains = nil;
  // If not nil, only file starting with `filename_prefix` are returned.
  NSString* filename_prefix = nil;
  // See
  // https://developers.google.com/drive/api/reference/rest/v3/files/list#query-parameters.
  // `extra_term` should not contain any term related to 'trashed', 'contains',
  // 'parents' or 'fullText', which are handled based on other properties.
  // `extra_term` is parenthesized and anded with the other terms. For example,
  // extraTerm could be:
  // @"mimeType = 'image/jpg' or mimeType = 'image/png'".
  NSString* extra_term = nil;
  // See
  // https://developers.google.com/drive/api/reference/rest/v3/files/list#query-parameters.
  // Order by which to sort the results.
  NSString* order_by = nil;
  // The maximum number of items to return per page. Default value is 20.
  // If `pageSize <= 0`, then the Drive API default page size will be used.
  // See
  // https://developers.google.com/drive/api/reference/rest/v3/files/list#query-parameters.
  NSInteger page_size = 20;
  // If not nil, the page token to use to continue a previous list/search. The
  // other fields in this object need to be the same as in previous requests.
  NSString* page_token = nil;
};

using DriveListCompletionCallback =
    base::OnceCallback<void(const DriveListResult&)>;

// This interface is used to perform list/search queries in a user's Drive.
class DriveList {
 public:
  DriveList();
  virtual ~DriveList();

  // Returns the identity used to perform queries.
  virtual id<SystemIdentity> GetIdentity() const = 0;
  // Returns whether a query is currently being executed by this uploader.
  virtual bool IsExecutingQuery() const = 0;
  // Cancels the query currently being executed by this uploader.
  virtual void CancelCurrentQuery() = 0;
  // List items in Drive matching the given `query`.
  // The final result, including possible error details, is returned
  // asynchronously through `completion_callback`.
  // TODO(crbug.com/344812086): This is being replaced with `ListFiles`. When
  // subclasses do not override this method anymore, remove it.
  virtual void ListItems(const DriveListQuery& query,
                         DriveListCompletionCallback completion_callback);
  // List files and folders in Drive matching the given `query`.
  // The final result, including possible error details, is returned
  // asynchronously through `completion_callback`.
  // TODO(crbug.com/344812086): Make pure virtual once implemented everywhere.
  virtual void ListFiles(const DriveListQuery& query,
                         DriveListCompletionCallback completion_callback);
  // List shared drives in Drive matching the given `query`.
  // The final result, including possible error details, is returned
  // asynchronously through `completion_callback`.
  // TODO(crbug.com/344812086): Make pure virtual once implemented everywhere.
  virtual void ListSharedDrives(
      const DriveListQuery& query,
      DriveListCompletionCallback completion_callback);
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_LIST_H_
