// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary MemoryInfo {
  // The total amount of physical memory capacity, in bytes.
  required double capacity;
  // The amount of available capacity, in bytes.
  required double availableCapacity;
};

// The <code>chrome.system.memory</code> API.
interface Memory {
  // Get physical memory information.
  // |PromiseValue|: info
  [requiredCallback] static Promise<MemoryInfo> getInfo();
};

partial interface System {
  static attribute Memory memory;
};

partial interface Browser {
  static attribute System system;
};
