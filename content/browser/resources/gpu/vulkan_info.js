// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('gpu', function() {
  class VulkanInfo {
    constructor(base64Data) {
      const array = Uint8Array.from(atob(base64Data), c => c.charCodeAt(0));
      const dataView = new DataView(array.buffer);
      this.vulkanInfo_ = gpu.mojom.VulkanInfo_Deserialize(dataView);
    }

    toString() {
      return JSON.stringify(this.vulkanInfo_, null, 2);
    }
  }

  return {VulkanInfo: VulkanInfo};
});
