// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary NetworkInterface {
  // The underlying name of the adapter. On *nix, this will typically be
  // "eth0", "wlan0", etc.
  required DOMString name;

  // The available IPv4/6 address.
  required DOMString address;

  // The prefix length
  required long prefixLength;
};

// Use the <code>chrome.system.network</code> API.
interface Network {
  // Retrieves information about local adapters on this system.
  // |Returns|: Called when local adapter information is available.
  // |PromiseValue|: networkInterfaces: Array of object containing network
  // interfaces information.
  [requiredCallback] static Promise<sequence<NetworkInterface>> getNetworkInterfaces();
};

partial interface System {
  static attribute Network network;
};

partial interface Browser {
  static attribute System system;
};
