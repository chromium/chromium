// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.transit;

import android.graphics.Rect;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.InstrumentationThreadCondition;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;
import java.util.function.Supplier;

/** {@link Condition}s related to HTML DOM Elements. */
public class HtmlConditions {
    /**
     * Fulfilled when a single DOM Element with the given id exists and has non-zero bounds in a
     * WebContents that isn't hidden or destroyed.
     *
     * <p>If {@code inViewport} is true, the DOM Element also needs to intersect the viewport.
     */
    public static class DisplayedCondition extends ConditionWithResult<Rect> {
        private final String mHtmlId;
        private final Supplier<WebContents> mWebContentsSupplier;
        private final boolean mInViewport;

        public DisplayedCondition(
                Supplier<WebContents> webContentsSupplier, String htmlId, boolean inViewport) {
            super(/* isRunOnUiThread= */ false);
            mWebContentsSupplier = dependOnSupplier(webContentsSupplier, "WebContents");
            mHtmlId = htmlId;
            mInViewport = inViewport;
        }

        @Override
        protected ConditionStatusWithResult<Rect> resolveWithSuppliers() throws Exception {
            WebContents webContents = mWebContentsSupplier.get();
            if (isDestroyedOrHidden(webContents)) {
                return notFulfilled("WebContents destroyed or hidden").withoutResult();
            }

            Rect bounds;
            try {
                bounds = DOMUtils.getNodeBounds(webContents, mHtmlId);
            } catch (AssertionError e) {
                // HTML elements might not exist yet, but will be created.
                return notFulfilled("getNodeBounds() threw assertion").withoutResult();
            }

            if (bounds.isEmpty()) {
                return notFulfilled("Bounds: %s", bounds.toShortString()).withoutResult();
            }

            if (!mInViewport) {
                return fulfilled("Bounds: %s", bounds.toShortString()).withResult(bounds);
            }

            Rect nodeClientRect;
            try {
                nodeClientRect = DOMUtils.getNodeClientRect(webContents, mHtmlId);
            } catch (AssertionError e) {
                return error("getNodeClientRect() threw: " + e).withoutResult();
            }

            Rect viewport;
            try {
                viewport = DOMUtils.getDocumentViewport(webContents);
            } catch (AssertionError e) {
                return error("getDocumentViewport() threw: " + e).withoutResult();
            }

            if (!Rect.intersects(nodeClientRect, viewport)) {
                return notFulfilled(
                                "node client rect %s, not displayed in viewport %s",
                                nodeClientRect.toShortString(), viewport.toShortString())
                        .withoutResult();
            }

            return fulfilled(
                            "Bounds: %s, client rect: %s, viewport: %s",
                            bounds.toShortString(),
                            nodeClientRect.toShortString(),
                            viewport.toShortString())
                    .withResult(bounds);
        }

        @Override
        public String buildDescription() {
            return String.format(
                    "DOM element with id=\"%s\" is displayed%s",
                    mHtmlId, mInViewport ? " in the viewport" : "");
        }
    }

    /**
     * Fulfilled when no DOM Elements with the given id exist with non-zero bounds, or when the
     * WebContents is null, destroyed or hidden.
     *
     * <p>If {@code inViewport} is true, also fulfilled when the DOM Element doesn't intersect the
     * viewport.
     */
    public static class NotDisplayedCondition extends InstrumentationThreadCondition {
        private final String mHtmlId;
        private final Supplier<WebContents> mWebContentsSupplier;
        private final boolean mInViewport;

        public NotDisplayedCondition(
                Supplier<WebContents> webContentsSupplier, String htmlId, boolean inViewport) {
            super();
            // Do not depend on purpose since if WebContents are not available, this Condition is
            // considered fulfilled.
            mWebContentsSupplier = webContentsSupplier;
            mHtmlId = htmlId;
            mInViewport = inViewport;
        }

        @Override
        public String buildDescription() {
            return String.format(
                    "No DOM element with id=\"%s\" is displayed%s",
                    mHtmlId, mInViewport ? " in the viewport" : "");
        }

        @Override
        protected ConditionStatus checkWithSuppliers() throws TimeoutException {
            WebContents webContents = mWebContentsSupplier.get();
            if (webContents == null) {
                return fulfilled("null webContents");
            }

            if (isDestroyedOrHidden(webContents)) {
                return fulfilled("WebContents destroyed or hidden");
            }

            Rect bounds;
            try {
                bounds = DOMUtils.getNodeBounds(webContents, mHtmlId);
            } catch (AssertionError e) {
                return fulfilled("getNodeBounds() threw assertion, object likely gone");
            }

            if (bounds.isEmpty()) {
                return fulfilled("Bounds: %s", bounds.toShortString());
            }

            if (!mInViewport) {
                return notFulfilled("Bounds: %s", bounds.toShortString());
            }

            Rect nodeClientRect;
            try {
                nodeClientRect = DOMUtils.getNodeClientRect(webContents, mHtmlId);
            } catch (AssertionError e) {
                return fulfilled("getNodeClientRect() threw assertion, object likely gone");
            }

            Rect viewport;
            try {
                viewport = DOMUtils.getDocumentViewport(webContents);
            } catch (AssertionError e) {
                return fulfilled("getDocumentViewport() threw assertion, object likely gone");
            }

            if (Rect.intersects(nodeClientRect, viewport)) {
                return notFulfilled(
                        "node client rect %s, displayed in viewport %s",
                        nodeClientRect.toShortString(), viewport.toShortString());
            }

            return fulfilled(
                    "Bounds: %s, client rect: %s, viewport: %s",
                    bounds.toShortString(),
                    nodeClientRect.toShortString(),
                    viewport.toShortString());
        }
    }

    // A hidden WebContents (e.g. the opener after a link opened a new tab) still lays out its DOM.
    private static boolean isDestroyedOrHidden(WebContents webContents) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        webContents.isDestroyed()
                                || webContents.getVisibility() == Visibility.HIDDEN);
    }
}
