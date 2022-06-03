// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_item.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "content/browser/download/save_file.h"
#include "content/browser/download/save_file_manager.h"
#include "content/browser/download/save_package.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

SaveItemId GetNextSaveItemId() {
  static SaveItemId::Generator g_save_item_id_generator;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_save_item_id_generator.GenerateNextId();
}

}  // namespace

// Constructor for SaveItem when creating each saving job.
SaveItem::SaveItem(const GURL& url,
                   const Referrer& referrer,
                   SavePackage* package,
                   SaveFileCreateInfo::SaveFileSource save_source,
                   int frame_tree_node_id,
                   int container_frame_tree_node_id)
    : save_item_id_(GetNextSaveItemId()),
      url_(url),
      referrer_(referrer),
      frame_tree_node_id_(frame_tree_node_id),
      container_frame_tree_node_id_(container_frame_tree_node_id),
      received_bytes_(0),
      state_(WAIT_START),
      is_success_(false),
      save_source_(save_source),
      package_(package) {
  DCHECK(package);
}

SaveItem::~SaveItem() {}

// Set start state for save item.
void SaveItem::Start() {
  DCHECK_EQ(state_, WAIT_START);
  state_ = IN_PROGRESS;
}

void SaveItem::UpdateSize(int64_t bytes_so_far) {
  received_bytes_ = bytes_so_far;
}

// Updates from the file thread may have been posted while this saving job
// was being canceled in the UI thread, so we'll accept them unless we're
// complete.
void SaveItem::Update(int64_t bytes_so_far) {
  if (state_ != IN_PROGRESS) {
    NOTREACHED();
    return;
  }
  UpdateSize(bytes_so_far);
}

// Cancel this saving item job. If the job is not in progress, ignore
// this command. The SavePackage will each in-progress SaveItem's cancel
// when canceling whole saving page job.
void SaveItem::Cancel() {
  // If item is in WAIT_START mode, which means no request has been sent.
  // So we need not to cancel it.
  if (state_ != IN_PROGRESS) {
    // Small downloads might be complete before method has a chance to run.
    return;
  }
  Finish(received_bytes_, false);
  state_ = CANCELED;
  package_->SaveCanceled(this);
}

// Set finish state for a save item
void SaveItem::Finish(int64_t size, bool is_success) {
  DCHECK(has_final_name() || !is_success_);
  state_ = COMPLETE;
  is_success_ = is_success;
  UpdateSize(size);
}

void SaveItem::SetTargetPath(const base::FilePath& full_path) {
  DCHECK(!full_path.empty());
  DCHECK(!has_final_name());
  full_path_ = full_path;
}

}  // namespace content
