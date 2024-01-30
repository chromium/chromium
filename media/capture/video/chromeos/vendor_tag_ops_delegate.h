// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_VENDOR_TAG_OPS_DELEGATE_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_VENDOR_TAG_OPS_DELEGATE_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/capture/video/chromeos/mojom/camera_common.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

struct VendorTagInfo {
  cros::mojom::CameraMetadataTag tag;
  std::string section_name;
  std::string tag_name;
  cros::mojom::EntryType type;
};

class VendorTagOpsDelegate {
 public:
  VendorTagOpsDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner);
  ~VendorTagOpsDelegate();

  // Setups/Teardowns the VendorTagOpsDelegate instance. All methods here should
  // be called on |ipc_task_runner_|.
  mojo::PendingReceiver<cros::mojom::VendorTagOps> MakeReceiver();
  void Initialize();
  void Reset();

  // Gets the info by name or tag after |inited_|. The returned pointer is still
  // owned by VendorTagOpsDelegate. Returns nullptr if not found. These
  // functions can be called concurrently on different threads.
  const VendorTagInfo* GetInfoByName(const std::string& full_name);
  const VendorTagInfo* GetInfoByTag(cros::mojom::CameraMetadataTag tag);

 private:
  void StopInitialization();
  void RemovePending(uint32_t tag);

  void OnGotTagCount(int32_t tag_count);
  void OnGotAllTags(size_t tag_count, const std::vector<uint32_t>& tags);
  void OnGotSectionName(uint32_t tag,
                        const std::optional<std::string>& section_name);
  void OnGotTagName(uint32_t tag, const std::optional<std::string>& tag_name);
  void OnGotTagType(uint32_t tag, int32_t type);

  scoped_refptr<base::SequencedTaskRunner> ipc_task_runner_;
  mojo::Remote<cros::mojom::VendorTagOps> vendor_tag_ops_;

  // The paritally initialized tags. A tag with its info would be moved to
  // |name_map_| and |tag_map_| once it's fully initialized. The |inited_| event
  // would be signaled when |pending_info_| is empty.
  std::map<uint32_t, VendorTagInfo> pending_info_;

  // These maps are read-only after |inited_|.
  std::map<std::string, VendorTagInfo> name_map_;
  std::map<cros::mojom::CameraMetadataTag, VendorTagInfo> tag_map_;

  base::WaitableEvent initialized_;

  base::Lock lock_;
  bool is_initializing_ GUARDED_BY(lock_);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_VENDOR_TAG_OPS_DELEGATE_H_
