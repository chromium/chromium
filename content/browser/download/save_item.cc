// Copyright 2012 The Chromium Authors
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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  return g_save_item_id_generator.GenerateNextId();
}

}  // namespace

// Constructor for SaveItem when creating each saving job.
SaveItem::SaveItem(const GURL& url,
                   const Referrer& referrer,
                   const net::IsolationInfo& isolation_info,
                   network::mojom::RequestMode request_mode,
                   bool is_outermost_main_frame,
                   SavePackage* package,
                   SaveFileCreateInfo::SaveFileSource save_source,
                   FrameTreeNodeId frame_tree_node_id,
                   FrameTreeNodeId container_frame_tree_node_id)
    : save_item_id_(GetNextSaveItemId()),
      url_(url),
      referrer_(referrer),
      isolation_info_(isolation_info),
      request_mode_(request_mode),
      is_outermost_main_frame_(is_outermost_main_frame),
      frame_tree_node_id_(frame_tree_node_id),
      container_frame_tree_node_id_(container_frame_tree_node_id),
      received_bytes_(0),
      state_(WAIT_START),
      is_success_(false),
      save_source_(save_source),
      package_(package) {
  CHECK(package, base::NotFatalUntil::M159);
}

SaveItem::~SaveItem() {}

// Set start state for save item.
void SaveItem::Start() {
  CHECK_EQ(state_, WAIT_START, base::NotFatalUntil::M159);
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
  CHECK(has_final_name() || !is_success_, base::NotFatalUntil::M159);
  state_ = COMPLETE;
  is_success_ = is_success;
  UpdateSize(size);
}

void SaveItem::SetTargetPath(const base::FilePath& full_path) {
  CHECK(!full_path.empty(), base::NotFatalUntil::M159);
  CHECK(!has_final_name(), base::NotFatalUntil::M159);
  full_path_ = full_path;
}

}  // namespace content
