// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/render_frame_host_test_support.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/runtime_feature_state/runtime_feature_state_document_data.h"

namespace content {

void LeaveInPendingDeletionState(RenderFrameHost* rfh) {
  static_cast<RenderFrameHostImpl*>(rfh)->DoNotDeleteForTesting();
}

bool IsDisableThirdPartyStoragePartitioningEnabled(RenderFrameHost* rfh) {
  DCHECK(rfh->IsInPrimaryMainFrame());

  RuntimeFeatureStateDocumentData* document_data =
      RuntimeFeatureStateDocumentData::GetForCurrentDocument(rfh);
  DCHECK(document_data);

  blink::RuntimeFeatureStateReadContext read_context =
      document_data->runtime_feature_state_read_context();

  return read_context.IsDisableThirdPartyStoragePartitioningEnabled();
}

}  // namespace content
