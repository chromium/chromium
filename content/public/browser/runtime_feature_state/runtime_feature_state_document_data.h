// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RUNTIME_FEATURE_STATE_RUNTIME_FEATURE_STATE_DOCUMENT_DATA_H_
#define CONTENT_PUBLIC_BROWSER_RUNTIME_FEATURE_STATE_RUNTIME_FEATURE_STATE_DOCUMENT_DATA_H_

#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_read_context.h"

namespace content {

class CONTENT_EXPORT RuntimeFeatureStateDocumentData
    : public DocumentUserData<RuntimeFeatureStateDocumentData> {
 public:
  ~RuntimeFeatureStateDocumentData() override;

  const blink::RuntimeFeatureStateReadContext&
  runtime_feature_state_read_context() {
    return runtime_feature_state_read_context_;
  }

  // We only want the read context to be mutable when an IPC is sent from the
  // renderer to the browser process requesting a feature diff be applied. Thus,
  // only the base::PassKey class OriginTrialStateHostImpl can access
  // this private function.
  blink::RuntimeFeatureStateReadContext&
  GetMutableRuntimeFeatureStateReadContext(
      base::PassKey<OriginTrialStateHostImpl>) {
    return runtime_feature_state_read_context_;
  }

 private:
  // No public constructors to force going through static methods of
  // DocumentUserData (e.g. CreateForCurrentDocument).
  RuntimeFeatureStateDocumentData(
      RenderFrameHost* rfh,
      const blink::RuntimeFeatureStateReadContext& read_context);
  // Use of this constructor is discouraged - callers of
  // RuntimeFeatureStateDocumentData should construct or inherit the
  // RuntimeFeatureStateReadContext at the callsite.
  // TODO(crbug.com/347186599): Determine the root cause of nullptr
  // RFSDocumentData and remove this constructor once fixed.
  RuntimeFeatureStateDocumentData(RenderFrameHost* rfh);

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  // The browser process' read-only copy containing the state for blink
  // runtime-enabled features.
  blink::RuntimeFeatureStateReadContext runtime_feature_state_read_context_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RUNTIME_FEATURE_STATE_RUNTIME_FEATURE_STATE_DOCUMENT_DATA_H_
