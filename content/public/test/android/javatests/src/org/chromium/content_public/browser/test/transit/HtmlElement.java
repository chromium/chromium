// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.transit;

import android.graphics.Rect;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.TravelException;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.transit.HtmlConditions.DisplayedCondition;
import org.chromium.content_public.browser.test.transit.HtmlConditions.NotDisplayedCondition;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;
import java.util.function.Supplier;

/**
 * A Public Transit {@link Element} representing an HTML DOM element to be searched for in the given
 * {@link WebContents}.
 */
public class HtmlElement extends Element<Rect> {
    protected final HtmlElementSpec mHtmlElementSpec;
    protected final Supplier<WebContents> mWebContentsSupplier;
    protected final Options mOptions;

    public HtmlElement(HtmlElementSpec htmlElementSpec, Supplier<WebContents> webContentsSupplier) {
        this(htmlElementSpec, webContentsSupplier, Options.DEFAULT);
    }

    public HtmlElement(
            HtmlElementSpec htmlElementSpec,
            Supplier<WebContents> webContentsSupplier,
            Options options) {
        super(htmlElementSpec.getId());
        mHtmlElementSpec = htmlElementSpec;
        mWebContentsSupplier = webContentsSupplier;
        mOptions = options;
    }

    /** Returns an Options builder to customize the HtmlElement further. */
    public static Options.Builder newOptions() {
        return new Options().new Builder();
    }

    @Override
    public @Nullable ConditionWithResult<Rect> createEnterCondition() {
        return new DisplayedCondition(
                mWebContentsSupplier, mHtmlElementSpec.getHtmlId(), mOptions.mInViewport);
    }

    @Override
    public Condition createExitCondition() {
        return new NotDisplayedCondition(
                mWebContentsSupplier, mHtmlElementSpec.getHtmlId(), mOptions.mInViewport);
    }

    /** Returns a trigger to click the HTML element to trigger a Transition. */
    // TODO(crbug.com/428032441): After scrolling, check that the element is in the viewport before
    // clicking or long pressing. On Desktop Android this needs the visual viewport offset.
    public Transition.Trigger getClickTrigger() {
        return () -> {
            try {
                DOMUtils.clickNode(mWebContentsSupplier.get(), mHtmlElementSpec.getHtmlId());
            } catch (TimeoutException e) {
                throw TravelException.newTravelException(
                        "Timed out trying to click DOM element", e);
            }
        };
    }

    /** Returns a trigger to long press the HTML element to trigger a Transition. */
    public Transition.Trigger getLongPressTrigger() {
        return () -> {
            try {
                DOMUtils.longPressNode(mWebContentsSupplier.get(), mHtmlElementSpec.getHtmlId());
            } catch (TimeoutException e) {
                throw TravelException.newTravelException(
                        "Timed out trying to long press DOM element", e);
            }
        };
    }

    /** Returns a TripBuilder with clicking the HTML element as trigger. */
    public TripBuilder clickTo() {
        return new TripBuilder().withContext(this).withTrigger(getClickTrigger());
    }

    /** Returns a TripBuilder with long pressing the HTML element as trigger. */
    public TripBuilder longPressTo() {
        return new TripBuilder().withContext(this).withTrigger(getLongPressTrigger());
    }

    /**
     * Extra options for declaring an {@link HtmlElement}.
     *
     * <p>By default, the DOM element is expected to exist and have non-zero bounds in a WebContents
     * that isn't hidden or destroyed. It can be outside the viewport: the click and long press
     * triggers scroll it into view first.
     */
    public static class Options {
        static final Options DEFAULT = new Options();
        protected boolean mInViewport;

        protected Options() {}

        public class Builder {
            public Options build() {
                return Options.this;
            }

            /**
             * Also expect the DOM element to intersect the viewport. It is then considered gone
             * once it no longer does, e.g. after scrolling it out of view.
             */
            public Builder inViewport() {
                mInViewport = true;
                return this;
            }
        }
    }

    /** Convenience {@link Options} setting inViewport(). */
    public static Options inViewportOption() {
        return newOptions().inViewport().build();
    }
}
