// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import BrowserEngineKit
import ExtensionFoundation
import Foundation

@main
class GPUProcess: NSObject, ChildProcessExtension, RenderingExtension {
  override required init() {
    super.init()
    ChildProcessStarted()
    ChildProcessInit(self)
    GpuProcessInit()
  }

  public func handle(xpcConnection: xpc_connection_t) {
    ChildProcessHandleNewConnection(xpcConnection)
  }

  public func applySandbox() {
    self.applyRestrictedSandbox(revision: .revision1)
  }
}
