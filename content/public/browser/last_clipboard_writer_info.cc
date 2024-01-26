// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/last_clipboard_writer_info.h"

#include "content/public/browser/render_frame_host.h"

namespace content {

namespace {

struct LastClipboardWriterInfo {
  // RFH ID of the last RFH to have committed data to the clipboard.
  GlobalRenderFrameHostId rfh_id;

  // The sequence number of the last commit made to the clipboard by the
  // browser.
  ui::ClipboardSequenceNumberToken seqno;
};

LastClipboardWriterInfo& LastClipboardWriterInfoStorage() {
  static LastClipboardWriterInfo info;
  return info;
}

}  // namespace

bool IsLastClipboardWrite(const RenderFrameHost& rfh,
                          ui::ClipboardSequenceNumberToken seqno) {
  const auto& info = LastClipboardWriterInfoStorage();
  return info.rfh_id == rfh.GetGlobalId() && info.seqno == seqno;
}

void SetLastClipboardWrite(const RenderFrameHost& rfh,
                           ui::ClipboardSequenceNumberToken seqno) {
  LastClipboardWriterInfoStorage() = {.rfh_id = rfh.GetGlobalId(),
                                      .seqno = std::move(seqno)};
}

void ClearIfLastClipboardWriterIs(const RenderFrameHost& rfh) {
  if (LastClipboardWriterInfoStorage().rfh_id == rfh.GetGlobalId()) {
    LastClipboardWriterInfoStorage() = {};
  }
}

}  // namespace content
