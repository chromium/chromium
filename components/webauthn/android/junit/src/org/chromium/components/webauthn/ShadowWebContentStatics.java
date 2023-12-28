// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;

@Implements(WebContentsStatics.class)
public class ShadowWebContentStatics extends WebContentsStatics {
    @Implementation
    public static WebContents fromRenderFrameHost(RenderFrameHost frameHost) {
        return null;
    }
}
