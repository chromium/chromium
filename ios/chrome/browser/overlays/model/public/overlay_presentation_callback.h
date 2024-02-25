// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_PRESENTATION_CALLBACK_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_PRESENTATION_CALLBACK_H_

#include "base/functional/callback.h"

// Overlay UI presented by OverlayPresenter::Delegate are provided with an
// OverlayPresentationCallback that is used to notify the presenter when
// requested overlay UI has finished being presented.
typedef base::OnceCallback<void()> OverlayPresentationCallback;

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_PRESENTATION_CALLBACK_H_
