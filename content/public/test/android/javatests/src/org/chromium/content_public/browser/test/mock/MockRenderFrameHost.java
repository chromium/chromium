// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.mock;

import org.chromium.base.Callback;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.services.service_manager.InterfaceProvider;
import org.chromium.url.Origin;

/**
 * Mock class for {@link RenderFrameHost}.
 */
public class MockRenderFrameHost implements RenderFrameHost {
    @Override
    public String getLastCommittedURL() {
        return null;
    }

    @Override
    public Origin getLastCommittedOrigin() {
        return null;
    }

    @Override
    public void getCanonicalUrlForSharing(Callback<String> callback) {}

    @Override
    public InterfaceProvider getRemoteInterfaces() {
        return null;
    }

    @Override
    public void notifyUserActivation() {}

    @Override
    public boolean isIncognito() {
        return false;
    }

    @Override
    public boolean isRenderFrameCreated() {
        return false;
    }

    @Override
    public boolean areInputEventsIgnored() {
        return false;
    }
}
