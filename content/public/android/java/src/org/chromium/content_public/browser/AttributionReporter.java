// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.content.browser.AttributionReporterImpl;

/**
 * Allows attributions to be reported independently from navigations.
 */
public abstract class AttributionReporter {
    private static AttributionReporter sAttributionReporterForTesting;

    /**
     * @return an AttributionReporter instance.
     */
    public static AttributionReporter getInstance() {
        if (sAttributionReporterForTesting != null) return sAttributionReporterForTesting;
        return new AttributionReporterImpl();
    }

    /**
     * Normally, Attributions should be reported through LoadUrlParams at the start of a navigation.
     * However, in some cases, like with speculative navigation, the attribution parameters aren't
     * available at the start of the navigation.
     *
     * This method allows Attributions to be reported for ongoing or already completed navigations,
     * as long as the current navigation finishes on the |destination| URL.
     *
     * @param webContents The WebContents the navigation to report an Attribution for is taking
     *         place in.
     * @see LoadUrlParams#setAttributionParameters for the rest of the parameters.
     */
    public abstract void reportAttributionForCurrentNavigation(WebContents webContents,
            String sourcePackageName, String sourceEventId, String destination, String reportTo,
            long expiry);

    /**
     * Report an Impression Attribution coming from an app - for example, when the user is shown an
     * ad in an app. This Attribution is not associated with any navigation.
     *
     * @param browserContext The BrowserContextHandle in which we're currently running.
     * @param eventTime The time at which the event took place in {@link System#currentTimeMillis()}
     *         timebase. Optional if the event was just received (and not cached).
     * @see LoadUrlParams#setAttributionParameters for the rest of the parameters.
     */
    public abstract void reportAppImpression(BrowserContextHandle browserContext,
            String sourcePackageName, String sourceEventId, String destination, String reportTo,
            long expiry, long eventTime);

    public static void setInstanceForTesting(AttributionReporter reporter) {
        sAttributionReporterForTesting = reporter;
    }
}
