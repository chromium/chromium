// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.transit;

import android.graphics.Rect;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.TravelException;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.transit.HtmlConditions.DisplayedCondition;
import org.chromium.content_public.browser.test.transit.HtmlConditions.NotDisplayedCondition;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/**
 * A Public Transit {@link Element} representing an HTML DOM element to be searched for in the given
 * {@link WebContents}.
 */
public class HtmlElement extends Element<Rect> {
    protected final HtmlElementSpec mHtmlElementSpec;
    protected final Supplier<WebContents> mWebContentsSupplier;

    public HtmlElement(HtmlElementSpec htmlElementSpec, Supplier<WebContents> webContentsSupplier) {
        super(htmlElementSpec.getId());
        mHtmlElementSpec = htmlElementSpec;
        mWebContentsSupplier = webContentsSupplier;
    }

    @Override
    public ConditionWithResult<Rect> createEnterCondition() {
        return new DisplayedCondition(mWebContentsSupplier, mHtmlElementSpec.getHtmlId());
    }

    @Override
    public Condition createExitCondition() {
        return new NotDisplayedCondition(mWebContentsSupplier, mHtmlElementSpec.getHtmlId());
    }

    /** Click the HTML element to trigger a Transition. */
    public void click() {
        try {
            DOMUtils.clickNode(mWebContentsSupplier.get(), mHtmlElementSpec.getHtmlId());
        } catch (TimeoutException e) {
            throw TravelException.newTravelException("Timed out trying to click DOM element", e);
        }
    }

    /** Long press the HTML element to trigger a Transition. */
    public void longPress() {
        try {
            DOMUtils.longPressNode(mWebContentsSupplier.get(), mHtmlElementSpec.getHtmlId());
        } catch (TimeoutException e) {
            throw TravelException.newTravelException(
                    "Timed out trying to long press DOM element", e);
        }
    }
}
