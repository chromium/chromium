// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.base.Callback;
import org.chromium.services.service_manager.InterfaceProvider;
import org.chromium.url.Origin;

/**
 * The RenderFrameHost Java wrapper to allow communicating with the native RenderFrameHost object.
 */
public interface RenderFrameHost {
    /**
     * Get the last committed URL of the frame.
     *
     * @return The last committed URL of the frame.
     */
    String getLastCommittedURL();

    /**
     * Get the last committed Origin of the frame. This is not always the same as scheme/host/port
     * of getLastCommittedURL(), since it can be an "opaque" origin in such cases as, for example,
     * sandboxed frame.
     *
     * @return The last committed Origin of the frame.
     */
    Origin getLastCommittedOrigin();

    /**
     * Fetch the canonical URL associated with the fame.
     *
     * @param callback The callback to be notified once the canonical URL has been fetched.
     */
    void getCanonicalUrlForSharing(Callback<String> callback);

    /**
     * Returns an InterfaceProvider that provides access to interface implementations provided by
     * the corresponding RenderFrame. This provides access to interfaces implemented in the renderer
     * to Java code in the browser process.
     *
     * @return The InterfaceProvider for the frame.
     */
    InterfaceProvider getRemoteInterfaces();

    /**
     * Notifies the native RenderFrameHost about a user activation from the browser side.
     */
    void notifyUserActivation();

    /**
     * Returns whether we're in incognito mode.
     *
     * @return {@code true} if we're in incoginto mode.
     */
    boolean isIncognito();

    /**
     * See native RenderFrameHost::IsRenderFrameCreated().
     *
     * @return {@code true} if render frame is created.
     */
    boolean isRenderFrameCreated();

    /**
     * @return Whether input events from the renderer are ignored on the browser side.
     */
    boolean areInputEventsIgnored();
}
