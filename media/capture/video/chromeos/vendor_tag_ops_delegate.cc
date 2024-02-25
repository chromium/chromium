// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/vendor_tag_ops_delegate.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"

namespace media {

VendorTagOpsDelegate::VendorTagOpsDelegate(
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner)
    : ipc_task_runner_(ipc_task_runner), is_initializing_(false) {}

VendorTagOpsDelegate::~VendorTagOpsDelegate() = default;

mojo::PendingReceiver<cros::mojom::VendorTagOps>
VendorTagOpsDelegate::MakeReceiver() {
  DCHECK(ipc_task_runner_->RunsTasksInCurrentSequence());
  auto receiver = vendor_tag_ops_.BindNewPipeAndPassReceiver();
  vendor_tag_ops_.set_disconnect_handler(
      base::BindOnce(&VendorTagOpsDelegate::Reset, base::Unretained(this)));
  return receiver;
}

void VendorTagOpsDelegate::Initialize() {
  DCHECK(ipc_task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock lock(lock_);
  is_initializing_ = true;
  vendor_tag_ops_->GetTagCount(base::BindOnce(
      &VendorTagOpsDelegate::OnGotTagCount, base::Unretained(this)));
}

void VendorTagOpsDelegate::Reset() {
  DCHECK(ipc_task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock lock(lock_);
  vendor_tag_ops_.reset();
  pending_info_.clear();
  name_map_.clear();
  tag_map_.clear();
  initialized_.Reset();
  is_initializing_ = false;
}

void VendorTagOpsDelegate::StopInitialization() {
  base::AutoLock lock(lock_);
  initialized_.Signal();
  is_initializing_ = false;
}

void VendorTagOpsDelegate::RemovePending(uint32_t tag) {
  DCHECK(ipc_task_runner_->RunsTasksInCurrentSequence());
  size_t removed = pending_info_.erase(tag);
  DCHECK_EQ(removed, 1u);
  if (pending_info_.empty()) {
    DVLOG(1) << "VendorTagOpsDelegate initialized";
    StopInitialization();
  }
}

void VendorTagOpsDelegate::OnGotTagCount(int32_t tag_count) {
  DCHECK(ipc_task_runner_->RunsTasksInCurrentSequence());
  if (tag_count == -1) {
    LOG(ERROR) << "Failed to get tag count";
    StopInitialization();
    return;
  }

  if (tag_count == 0) {
    // There is no vendor tag, we are done here.
    StopInitialization();
    return;
  }

  vendor_tag_ops_->GetAllTags(base::BindOnce(
      &VendorTagOpsDelegate::OnGotAllTags, base::Unretained(this), tag_count));
}

void VendorTagOpsDelegate::OnGotAllTags(size_t tag_count,
                                        const std::vector<uint32_t>& tags) {
  DCHECK(ipc_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(tags.size(), tag_count);

  for (uint32_t tag : tags) {
    pending_info_[tag].tag = static_cast<cros::mojom::CameraMetadataTag>(tag);
    vendor_tag_ops_->GetSectionName(
        tag, base::BindOnce(&VendorTagOpsDelegate::OnGotSectionName,
                            base::Unretained(this), tag));
  }
}

void VendorTagOpsDelegate::OnGotSectionName(
    uint32_t tag,
    const std::optional<std::string>& section_name) {
  DCHECK(ipc_task_runner_->RunsTasksInCurrentSequence());
  if (!section_name.has_value()) {
    LOG(ERROR) << "Failed to get section name of tag " << std::hex
               << std::showbase << tag;
    RemovePending(tag);
    return;
  }

  pending_info_[tag].section_name = *section_name;
  vendor_tag_ops_->GetTagName(
      tag, base::BindOnce(&VendorTagOpsDelegate::OnGotTagName,
                          base::Unretained(this), tag));
}

void VendorTagOpsDelegate::OnGotTagName(
    uint32_t tag,
    const std::optional<std::string>& tag_name) {
  DCHECK(ipc_task_runner_->RunsTasksInCurrentSequence());
  if (!tag_name.has_value()) {
    LOG(ERROR) << "Failed to get tag name of tag " << std::hex << std::showbase
               << tag;
    RemovePending(tag);
    return;
  }

  pending_info_[tag].tag_name = *tag_name;
  vendor_tag_ops_->GetTagType(
      tag, base::BindOnce(&VendorTagOpsDelegate::OnGotTagType,
                          base::Unretained(this), tag));
}

void VendorTagOpsDelegate::OnGotTagType(uint32_t tag, int32_t type) {
  DCHECK(ipc_task_runner_->RunsTasksInCurrentSequence());
  if (type == -1) {
    LOG(ERROR) << "Failed to get tag type of tag " << std::hex << std::showbase
               << tag;
    RemovePending(tag);
    return;
  }

  VendorTagInfo& info = pending_info_[tag];
  info.type = static_cast<cros::mojom::EntryType>(type);
  std::string full_name = base::StrCat({info.section_name, ".", info.tag_name});
  name_map_[full_name] = info;
  RemovePending(tag);
}

const VendorTagInfo* VendorTagOpsDelegate::GetInfoByName(
    const std::string& full_name) {
  {
    base::AutoLock lock(lock_);
    if (!is_initializing_ && !initialized_.IsSignaled()) {
      LOG(WARNING) << "VendorTagOps is accessed before calling Initialize()";
      return nullptr;
    }
  }
  initialized_.Wait();
  auto it = name_map_.find(full_name);
  if (it == name_map_.end()) {
    return nullptr;
  }
  return &it->second;
}

const VendorTagInfo* VendorTagOpsDelegate::GetInfoByTag(
    cros::mojom::CameraMetadataTag tag) {
  {
    base::AutoLock lock(lock_);
    if (!is_initializing_ && !initialized_.IsSignaled()) {
      LOG(WARNING) << "VendorTagOps is accessed before calling Initialize()";
      return nullptr;
    }
  }
  initialized_.Wait();
  auto it = tag_map_.find(tag);
  if (it == tag_map_.end()) {
    return nullptr;
  }
  return &it->second;
}

}  // namespace media
