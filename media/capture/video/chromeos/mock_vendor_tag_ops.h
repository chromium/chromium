// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_MOCK_VENDOR_TAG_OPS_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_MOCK_VENDOR_TAG_OPS_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/threading/thread.h"
#include "media/capture/video/chromeos/mojom/camera_common.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace unittest_internal {

class MockVendorTagOps : public cros::mojom::VendorTagOps {
 public:
  MockVendorTagOps();
  ~MockVendorTagOps() override;

  void Bind(mojo::PendingReceiver<cros::mojom::VendorTagOps> receiver);

  MOCK_METHOD0(DoGetTagCount, int32_t());
  void GetTagCount(GetTagCountCallback callback) override;

  MOCK_METHOD0(DoGetAllTags, std::vector<uint32_t>());
  void GetAllTags(GetAllTagsCallback callback) override;

  MOCK_METHOD1(DoGetSectionName, std::optional<std::string>(uint32_t tag));
  void GetSectionName(uint32_t tag, GetSectionNameCallback callback) override;

  MOCK_METHOD1(DoGetTagName, std::optional<std::string>(uint32_t tag));
  void GetTagName(uint32_t tag, GetTagNameCallback callback) override;

  MOCK_METHOD1(DoGetTagType, int32_t(uint32_t tag));
  void GetTagType(uint32_t tag, GetTagTypeCallback callback) override {
    std::move(callback).Run(DoGetTagType(tag));
  }

 private:
  void CloseBindingOnThread();

  void BindOnThread(base::WaitableEvent* done,
                    mojo::PendingReceiver<cros::mojom::VendorTagOps> receiver);

  base::Thread mock_vendor_tag_ops_thread_;
  mojo::Receiver<cros::mojom::VendorTagOps> receiver_{this};
};

}  // namespace unittest_internal
}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_MOCK_VENDOR_TAG_OPS_H_
