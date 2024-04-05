// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.transit;

import android.graphics.Rect;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.InstrumentationThreadCondition;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/** {@link Condition}s related to HTML DOM Elements. */
public class HtmlConditions {
    /** Fulfilled when a single DOM Element with the given id exists and has non-zero bounds. */
    public static class DisplayedCondition extends InstrumentationThreadCondition {
        private final String mHtmlId;
        private final WebContentsElementInState mWebContentsElementInState;

        public DisplayedCondition(
                WebContentsElementInState webContentsElementInState, String htmlId) {
            super();
            mWebContentsElementInState = webContentsElementInState;
            mHtmlId = htmlId;
        }

        @Override
        public ConditionStatus check() throws Exception {
            WebContents webContents = mWebContentsElementInState.getWebContents();
            if (webContents == null) {
                return notFulfilled("null webContents");
            }

            Rect bounds;
            try {
                bounds = DOMUtils.getNodeBounds(webContents, mHtmlId);
            } catch (AssertionError e) {
                // HTML elements might not exist yet, but will be created.
                return notFulfilled("getNodeBounds() threw assertion");
            }

            return whether(!bounds.isEmpty(), "Bounds: %s", bounds.toShortString());
        }

        @Override
        public String buildDescription() {
            return String.format("DOM element with id=\"%s\" is displayed", mHtmlId);
        }
    }

    /** Fulfilled when no DOM Elements with the given id exist with non-zero bounds. */
    public static class NotDisplayedCondition extends InstrumentationThreadCondition {
        private final String mHtmlId;
        private final WebContentsElementInState mWebContentsElementInState;

        public NotDisplayedCondition(
                WebContentsElementInState webContentsElementInState, String htmlId) {
            super();
            mWebContentsElementInState = webContentsElementInState;
            mHtmlId = htmlId;
        }

        @Override
        public String buildDescription() {
            return String.format("No DOM element with id=\"%s\" is displayed", mHtmlId);
        }

        @Override
        public ConditionStatus check() throws TimeoutException {
            WebContents webContents = mWebContentsElementInState.getWebContents();
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
