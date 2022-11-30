// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_READING_LIST_MODAL_OVERLAY_RESPONSES_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_READING_LIST_MODAL_OVERLAY_RESPONSES_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/overlays/public/overlay_response_info.h"

namespace reading_list_infobar_modal_responses {

// Response info used to create dispatched OverlayResponses that notify the
// add to reading list infobar to never prompt.
DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(NeverAsk);

}  // namespace reading_list_infobar_modal_responses

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_READING_LIST_MODAL_OVERLAY_RESPONSES_H_
