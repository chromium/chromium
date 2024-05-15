// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import BrowserEngineKit
import ExtensionFoundation
import Foundation

@main
class ContentProcess: WebContentExtension {
  required init() {
    ChildProcessInit()
  }

  public func handle(xpcConnection: xpc_connection_t) {
    ChildProcessHandleNewConnection(xpcConnection)
  }
}
