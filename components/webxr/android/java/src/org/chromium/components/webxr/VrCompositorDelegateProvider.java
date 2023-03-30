// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import org.chromium.content_public.browser.WebContents;

/**
 * Interface used to create instances of VrCompositorDelegates, needed to talk
 * to the compositor to ensure that the Surface renders correctly.
 */
public interface VrCompositorDelegateProvider {
    VrCompositorDelegate create(WebContents webContents);
}
