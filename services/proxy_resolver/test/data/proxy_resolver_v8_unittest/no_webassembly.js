// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function FindProxyForURL(url, host) {
  if (typeof WebAssembly == "undefined") return "DIRECT";
  throw new Error("WebAssembly should not be available");
}

