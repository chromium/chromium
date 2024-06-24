// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.transit;

import android.graphics.Rect;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.ElementInState;
import org.chromium.base.test.transit.TravelException;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.transit.HtmlConditions.DisplayedCondition;
import org.chromium.content_public.browser.test.transit.HtmlConditions.NotDisplayedCondition;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/**
 * A Public Transit ElementInState representing an HTML DOM element to be searched for in the given
 * WebContentsElementInState.
 */
public class HtmlElementInState extends ElementInState<Rect> {
    protected final HtmlElement mHtmlElement;
    protected final Supplier<WebContents> mWebContentsSupplier;

    public HtmlElementInState(HtmlElement htmlElement, Supplier<WebContents> webContentsSupplier) {
        super(htmlElement.getId());
        mHtmlElement = htmlElement;
        mWebContentsSupplier = webContentsSupplier;
    }

    @Override
    public ConditionWithResult<Rect> createEnterCondition() {
        return new DisplayedCondition(mWebContentsSupplier, mHtmlElement.getHtmlId());
    }

    @Override
    public Condition createExitCondition() {
        return new NotDisplayedCondition(mWebContentsSupplier, mHtmlElement.getHtmlId());
    }

    /** Click the HTML element to trigger a Transition. */
    public void click() {
        try {
            DOMUtils.clickNode(mWebContentsSupplier.get(), mHtmlElement.getHtmlId());
        } catch (TimeoutException e) {
            throw TravelException.newTravelException("Timed out trying to click DOM element", e);
        }
    }
}
