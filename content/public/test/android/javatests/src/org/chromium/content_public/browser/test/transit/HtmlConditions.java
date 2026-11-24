// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.transit;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Rect;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.InstrumentationThreadCondition;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;
import java.util.function.Supplier;

/** {@link Condition}s related to HTML DOM Elements. */
@NullMarked
public class HtmlConditions {
    /**
     * Fulfilled when the first DOM Element matching the given {@link HtmlElementSpec} exists and
     * has non-zero bounds in a WebContents that isn't hidden or destroyed.
     *
     * <p>If {@code inViewport} is true, the DOM Element also needs to intersect the viewport.
     */
    public static class DisplayedCondition extends ConditionWithResult<Rect> {
        private final HtmlElementSpec mSpec;
        private final Supplier<@Nullable WebContents> mWebContentsSupplier;
        private final boolean mInViewport;

        public DisplayedCondition(
                Supplier<@Nullable WebContents> webContentsSupplier,
                HtmlElementSpec spec,
                boolean inViewport) {
            super(/* isRunOnUiThread= */ false);
            mWebContentsSupplier = dependOnSupplier(webContentsSupplier, "WebContents");
            mSpec = spec;
            mInViewport = inViewport;
        }

        @Override
        protected ConditionStatusWithResult<Rect> resolveWithSuppliers() throws Exception {
            WebContents webContents = assumeNonNull(mWebContentsSupplier.get());
            if (isDestroyedOrHidden(webContents)) {
                return notFulfilled("WebContents destroyed or hidden").withoutResult();
            }

            Rect bounds = DOMUtils.getNodeBoundsByJsOrNull(webContents, mSpec.getSelectorJs());
            if (bounds == null) {
                return notFulfilled("Element not found").withoutResult();
            }

            if (bounds.isEmpty()) {
                return notFulfilled("Bounds: %s", bounds.toShortString()).withoutResult();
            }

            if (!mInViewport) {
                return fulfilled("Bounds: %s", bounds.toShortString()).withResult(bounds);
            }

            Rect nodeClientRect =
                    DOMUtils.getNodeClientRectByJsOrNull(webContents, mSpec.getSelectorJs());
            if (nodeClientRect == null) {
                return notFulfilled("Element not found").withoutResult();
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
                    "DOM element with %s is displayed%s",
                    mSpec.getDescription(), mInViewport ? " in the viewport" : "");
        }
    }

    /**
     * Fulfilled when the first DOM Element matching the given {@link HtmlElementSpec} doesn't exist
     * or has zero bounds, or when the WebContents is null, destroyed or hidden.
     *
     * <p>If {@code inViewport} is true, also fulfilled when the DOM Element doesn't intersect the
     * viewport.
     */
    public static class NotDisplayedCondition extends InstrumentationThreadCondition {
        private final HtmlElementSpec mSpec;
        private final Supplier<@Nullable WebContents> mWebContentsSupplier;
        private final boolean mInViewport;

        public NotDisplayedCondition(
                Supplier<@Nullable WebContents> webContentsSupplier,
                HtmlElementSpec spec,
                boolean inViewport) {
            super();
            // Do not depend on purpose since if WebContents are not available, this Condition is
            // considered fulfilled.
            mWebContentsSupplier = webContentsSupplier;
            mSpec = spec;
            mInViewport = inViewport;
        }

        @Override
        public String buildDescription() {
            return String.format(
                    "No DOM element with %s is displayed%s",
                    mSpec.getDescription(), mInViewport ? " in the viewport" : "");
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

            Rect bounds = DOMUtils.getNodeBoundsByJsOrNull(webContents, mSpec.getSelectorJs());
            if (bounds == null) {
                return fulfilled("Element not found");
            }

            if (bounds.isEmpty()) {
                return fulfilled("Bounds: %s", bounds.toShortString());
            }

            if (!mInViewport) {
                return notFulfilled("Bounds: %s", bounds.toShortString());
            }

            Rect nodeClientRect =
                    DOMUtils.getNodeClientRectByJsOrNull(webContents, mSpec.getSelectorJs());
            if (nodeClientRect == null) {
                return fulfilled("Element not found");
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

    /**
     * Fulfilled when the first DOM Element matching the given {@link HtmlElementSpec} exists and
     * its string property (e.g. {@code value}, {@code textContent}) equals {@code expected} in a
     * WebContents that isn't hidden or destroyed.
     */
    public static class StringPropertyCondition extends InstrumentationThreadCondition {
        private final Supplier<@Nullable WebContents> mWebContentsSupplier;
        private final HtmlElementSpec mSpec;
        private final String mPropertyName;
        private final String mExpected;

        public StringPropertyCondition(
                Supplier<@Nullable WebContents> webContentsSupplier,
                HtmlElementSpec spec,
                String propertyName,
                String expected) {
            mWebContentsSupplier = dependOnSupplier(webContentsSupplier, "WebContents");
            mSpec = spec;
            mPropertyName = propertyName;
            mExpected = expected;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() throws TimeoutException {
            WebContents webContents = assumeNonNull(mWebContentsSupplier.get());
            if (isDestroyedOrHidden(webContents)) {
                return notFulfilled("WebContents destroyed or hidden");
            }

            String actual =
                    DOMUtils.getNodeFieldByJsOrNull(
                            webContents, mSpec.getSelectorJs(), mPropertyName);
            if (actual == null) {
                return notFulfilled("Element not found or %s is null", mPropertyName);
            }
            return whetherEquals(mExpected, actual);
        }

        @Override
        public String buildDescription() {
            return String.format(
                    "DOM element with %s has %s \"%s\"",
                    mSpec.getDescription(), mPropertyName, mExpected);
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
