// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.AttributionReporter;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;

/**
 * Allows attributions to be reported independently from navigations.
 */
@JNINamespace("content")
public class AttributionReporterImpl extends AttributionReporter {
    /**
     * @see AttributionReporter#reportAttributionForCurrentNavigation.
     */
    @Override
    public void reportAttributionForCurrentNavigation(WebContents webContents,
            String sourcePackageName, String sourceEventId, String destination, String reportTo,
            long expiry) {
        AttributionReporterImplJni.get().reportAttributionForCurrentNavigation(
                webContents, sourcePackageName, sourceEventId, destination, reportTo, expiry);
    }

    /**
     * @see AttributionReporter#reportAppImpression.
     */
    @Override
    public void reportAppImpression(BrowserContextHandle browserContext, String sourcePackageName,
            String sourceEventId, String destination, String reportTo, long expiry,
            long eventTime) {
        AttributionReporterImplJni.get().reportAppImpression(browserContext, sourcePackageName,
                sourceEventId, destination, reportTo, expiry, eventTime);
    }

    @NativeMethods
    interface Natives {
        void reportAttributionForCurrentNavigation(WebContents webContents,
                String sourcePackageName, String sourceEventId, String destination, String reportTo,
                long expiry);
        void reportAppImpression(BrowserContextHandle browserContext, String sourcePackageName,
                String sourceEventId, String destination, String reportTo, long expiry,
                long eventTime);
    }
}
