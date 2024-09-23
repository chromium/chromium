// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;

/** Provides helper methods to add GPM-specific flags to credential requests. */
public class GpmBrowserOptionsHelper {
    private static final String CHANNEL_KEY = "com.android.chrome.CHANNEL";
    private static final String INCOGNITO_KEY = "com.android.chrome.INCOGNITO";
    private static Boolean sIsIncognitoForTesting;

    /**
     * Adds the channel info so that GPM can (depending on context and request):
     *
     * <ul>
     *   <li>prioritize the credentials for the current account in Chrome,
     *   <li>return passwords only for that channel, or
     *   <li>save credential to the correct account.
     * </ul>
     *
     * @param browserOptions A {@link Bundle} of browser-specific options.
     */
    public static void addChannelExtraToOptions(Bundle browserOptions) {
        browserOptions.putString(CHANNEL_KEY, getChannel());
    }

    /**
     * Specify if the tab is in incognito mode for user privacy.
     *
     * @param browserOptions A {@link Bundle} of browser-specific options.
     * @param renderFrameHost The frame from which the request is made.
     */
    public static void addIncognitoExtraToOptions(
            Bundle browserOptions, RenderFrameHost renderFrameHost) {
        browserOptions.putBoolean(INCOGNITO_KEY, isIncognito(renderFrameHost));
    }

    /**
     * Constructs a bundle for Browser Options with minimal information.
     *
     * @return A {@link Bundle} of browser-specific options including the options.
     */
    public static Bundle createDefaultBrowserOptions() {
        Bundle browserOptions = new Bundle();
        addChannelExtraToOptions(browserOptions);
        return browserOptions;
    }

    private static final String getChannel() {
        if (VersionInfo.isCanaryBuild()) {
            return "canary";
        }
        if (VersionInfo.isDevBuild()) {
            return "dev";
        }
        if (VersionInfo.isBetaBuild()) {
            return "beta";
        }
        if (VersionInfo.isStableBuild()) {
            return "stable";
        }
        if (VersionInfo.isLocalBuild()) {
            return "built_locally";
        }
        assert false : "Channel must be canary, dev, beta, stable or chrome must be built locally.";
        return null;
    }

    private static final boolean isIncognito(RenderFrameHost frameHost) {
        if (sIsIncognitoForTesting != null) return sIsIncognitoForTesting;
        if (frameHost == null) return false;
        WebContents webContents = WebContentsStatics.fromRenderFrameHost(frameHost);
        return webContents == null ? false : webContents.isIncognito();
    }

    @VisibleForTesting
    public static void setIsIncognitoExtraUntilTearDown(Boolean isIncognito) {
        sIsIncognitoForTesting = isIncognito;
        ResettersForTesting.register(() -> sIsIncognitoForTesting = null);
    }

    private GpmBrowserOptionsHelper() {}
}
