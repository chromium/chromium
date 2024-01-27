// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_LAST_CLIPBOARD_WRITER_INFO_H_
#define CONTENT_PUBLIC_BROWSER_LAST_CLIPBOARD_WRITER_INFO_H_

#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

class RenderFrameHost;

// Helpers to check if an `rfh`/`seqno` pair was the last browser tab to write
// to the clipboard.
CONTENT_EXPORT bool IsLastClipboardWrite(
    const RenderFrameHost& rfh,
    ui::ClipboardSequenceNumberToken seqno);

CONTENT_EXPORT void SetLastClipboardWrite(
    const RenderFrameHost& rfh,
    ui::ClipboardSequenceNumberToken seqno);

CONTENT_EXPORT void ClearIfLastClipboardWriterIs(const RenderFrameHost& rfh);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_LAST_CLIPBOARD_WRITER_INFO_H_
