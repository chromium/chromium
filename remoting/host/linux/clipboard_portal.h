// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_CLIPBOARD_PORTAL_H_
#define REMOTING_HOST_LINUX_CLIPBOARD_PORTAL_H_

#include "remoting/host/clipboard.h"

namespace remoting {

class ClipboardPortal : public Clipboard {
 public:
  ClipboardPortal();
  ~ClipboardPortal() override;

  // Clipboard interface.
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_CLIPBOARD_PORTAL_H_
