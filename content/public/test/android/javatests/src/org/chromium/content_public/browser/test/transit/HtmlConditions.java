// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.transit;

import android.graphics.Rect;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.InstrumentationThreadCondition;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/** {@link Condition}s related to HTML DOM Elements. */
public class HtmlConditions {
    /** Fulfilled when a single DOM Element with the given id exists and has non-zero bounds. */
    public static class DisplayedCondition extends ConditionWithResult<Rect> {
        private final String mHtmlId;
        private final Supplier<WebContents> mWebContentsCondition;

        public DisplayedCondition(Supplier<WebContents> webContentsSupplier, String htmlId) {
            super(/* isRunOnUiThread= */ false);
            mWebContentsCondition = dependOnSupplier(webContentsSupplier, "WebContents");
            mHtmlId = htmlId;
        }

        @Override
        protected ConditionStatusWithResult<Rect> resolveWithSuppliers() throws Exception {
            Rect bounds;
            try {
                bounds = DOMUtils.getNodeBounds(mWebContentsCondition.get(), mHtmlId);
            } catch (AssertionError e) {
                // HTML elements might not exist yet, but will be created.
                return notFulfilled("getNodeBounds() threw assertion").withoutResult();
            }

            return whether(!bounds.isEmpty(), "Bounds: %s", bounds.toShortString())
                    .withResult(bounds);
        }

        @Override
        public String buildDescription() {
            return String.format("DOM element with id=\"%s\" is displayed", mHtmlId);
        }
    }

    /** Fulfilled when no DOM Elements with the given id exist with non-zero bounds. */
    public static class NotDisplayedCondition extends InstrumentationThreadCondition {
        private final String mHtmlId;
        private final Supplier<WebContents> mWebContentsSupplier;

        public NotDisplayedCondition(Supplier<WebContents> webContentsSupplier, String htmlId) {
            super();
            // Do not depend on purpose since if WebContents are not available, this Condition is
            // considered fulfilled.
            mWebContentsSupplier = webContentsSupplier;
            mHtmlId = htmlId;
        }

        @Override
        public String buildDescription() {
            return String.format("No DOM element with id=\"%s\" is displayed", mHtmlId);
        }

        @Override
        protected ConditionStatus checkWithSuppliers() throws TimeoutException {
            WebContents webContents = mWebContentsSupplier.get();
            if (webContents == null) {
                return fulfilled("null webContents");
            }

            Rect bounds;
            try {
                bounds = DOMUtils.getNodeBounds(webContents, mHtmlId);
            } catch (AssertionError e) {
                return fulfilled("getNodeBounds() threw assertion, object likely gone");
            }

            return whether(bounds.isEmpty(), "Bounds: %s", bounds.toShortString());
        }
    }
}
