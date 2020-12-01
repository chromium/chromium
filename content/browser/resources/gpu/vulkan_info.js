// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VulkanInfo_Deserialize} from './vulkan_info.mojom-webui.js';

export class VulkanInfo {
  constructor(base64Data) {
    const array = Uint8Array.from(atob(base64Data), c => c.charCodeAt(0));
    const dataView = new DataView(array.buffer);
    this.vulkanInfo_ = VulkanInfo_Deserialize(dataView);
  }

  toString() {
    return JSON.stringify(this.vulkanInfo_, null, 2);
  }
}
