// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRESENTATION_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_PRESENTATION_OBSERVER_H_

#include "base/observer_list_types.h"
#include "content/common/content_export.h"

namespace content {

struct PresentationRequest;

// Observes changes to presentations associated with a WebContents.
class CONTENT_EXPORT PresentationObserver : public base::CheckedObserver {
 public:
  // Called whenever presentations are added or removed.
  virtual void OnPresentationsChanged(bool has_presentation) {}

  // `presentation_request` is a nullptr if the default PresentationRequest
  // has been removed.
  virtual void OnDefaultPresentationChanged(
      const content::PresentationRequest* presentation_request) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRESENTATION_OBSERVER_H_
