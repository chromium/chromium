// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_ITEM_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_ITEM_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/browser/download/save_types.h"
#include "content/public/common/referrer.h"
#include "url/gurl.h"

namespace content {
class SavePackage;

// One SaveItem per save file. This is the model class that stores all the
// state for one save file.
class SaveItem {
 public:
  enum SaveState {
    WAIT_START,
    IN_PROGRESS,
    COMPLETE,
    CANCELED
  };

  SaveItem(const GURL& url,
           const Referrer& referrer,
           SavePackage* package,
           SaveFileCreateInfo::SaveFileSource save_source,
           int frame_tree_node_id,
           int container_frame_tree_node_id);

  ~SaveItem();

  void Start();

  // Received a new chunk of data.
  void Update(int64_t bytes_so_far);

  // Cancel saving item.
  void Cancel();

  // Saving operation completed.
  void Finish(int64_t size, bool is_success);

  // Update path for SaveItem, the actual file is renamed on the file thread.
  void SetTargetPath(const base::FilePath& full_path);

  // Accessors.
  SaveItemId id() const { return save_item_id_; }
  SaveState state() const { return state_; }
  const base::FilePath& full_path() const { return full_path_; }
  const GURL& url() const { return url_; }
  const Referrer& referrer() const { return referrer_; }
  int frame_tree_node_id() const { return frame_tree_node_id_; }
  int container_frame_tree_node_id() const {
    return container_frame_tree_node_id_;
  }
  int64_t received_bytes() const { return received_bytes_; }
  bool has_final_name() const { return !full_path_.empty(); }
  bool success() const { return is_success_; }
  SaveFileCreateInfo::SaveFileSource save_source() const {
    return save_source_;
  }

 private:
  // Internal helper for maintaining consistent received and total sizes.
  void UpdateSize(int64_t size);

  // Unique identifier for this SaveItem instance.
  const SaveItemId save_item_id_;

  // Full path to the save item file.
  base::FilePath full_path_;

  // The URL for this save item.
  GURL url_;
  Referrer referrer_;

  // Frame tree node id, if this save item represents a frame
  // (otherwise FrameTreeNode::kFrameTreeNodeInvalidId).
  int frame_tree_node_id_;

  // Frame tree node id of the frame containing this save item.
  // (FrameTreeNode::kFrameTreeNodeInvalidId if this save item represents the
  // main frame, which obviously doesn't have a containing/parent frame).
  int container_frame_tree_node_id_;

  // Current received bytes.
  int64_t received_bytes_;

  // The current state of this save item.
  SaveState state_;

  // Flag indicates whether SaveItem has error while in saving process.
  bool is_success_;

  SaveFileCreateInfo::SaveFileSource save_source_;

  // Our owning object.
  SavePackage* package_;

  DISALLOW_COPY_AND_ASSIGN(SaveItem);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_ITEM_H_
