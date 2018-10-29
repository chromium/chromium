// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SAVABLE_RESOURCES_H_
#define CONTENT_RENDERER_SAVABLE_RESOURCES_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/savable_subframe.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "url/gurl.h"

namespace blink {
class WebElement;
class WebLocalFrame;
class WebString;
}

// A collection of operations that access the underlying WebKit DOM directly.
namespace content {

// Structure for storage the result of getting all savable resource links
// for current page. The consumer of the SavableResourcesResult is responsible
// for keeping these pointers valid for the lifetime of the
// SavableResourcesResult instance.
struct SavableResourcesResult {
  // Links of all savable resources.
  std::vector<GURL>* resources_list;

  // Subframes.
  std::vector<SavableSubframe>* subframes;

  // Constructor.
  SavableResourcesResult(
      std::vector<GURL>* resources_list,
      std::vector<SavableSubframe>* subframes)
      : resources_list(resources_list),
        subframes(subframes) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SavableResourcesResult);
};

// Get all the savable resource links from the specified |frame|.
// Returns true if the saved resources links have been saved successfully.
// Otherwise returns false (i.e. if the frame contains a non-savable content).
CONTENT_EXPORT bool GetSavableResourceLinksForFrame(
    blink::WebLocalFrame* frame,
    SavableResourcesResult* result);

// Returns the value in an elements resource url attribute. For IMG, SCRIPT or
// INPUT TYPE=image, returns the value in "src". For LINK TYPE=text/css, returns
// the value in "href". For BODY, TABLE, TR, TD, returns the value in
// "background". For BLOCKQUOTE, Q, DEL, INS, returns the value in "cite"
// attribute. Otherwise returns a null WebString.
CONTENT_EXPORT blink::WebString GetSubResourceLinkFromElement(
    const blink::WebElement& element);

}  // namespace content

#endif  // CONTENT_RENDERER_SAVABLE_RESOURCES_H_
