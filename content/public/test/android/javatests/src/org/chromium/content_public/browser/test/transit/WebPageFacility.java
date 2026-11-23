// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.transit;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

/**
 * A {@link Facility} representing DOM elements inside a loaded web page.
 *
 * <p>Subclasses declare {@link HtmlElement}s as fields using {@link #declareHtmlElement(String)} or
 * {@link #declareHtmlElement(HtmlElementSpec)}. The host Station must implement {@link
 * WebContentsElementProvider}.
 */
@NullMarked
public abstract class WebPageFacility extends Facility<Station<?>> {
    /** Declares an {@link HtmlElement} matching {@code cssSelector}. */
    protected HtmlElement declareHtmlElement(String cssSelector) {
        return declareHtmlElement(new HtmlElementSpec(cssSelector));
    }

    /** Declares an {@link HtmlElement} matching {@code spec}. */
    protected HtmlElement declareHtmlElement(HtmlElementSpec spec) {
        return declareElement(new HtmlElement(spec, this::getWebContents));
    }

    private @Nullable WebContents getWebContents() {
        // Throw an AssertionError rather than an Exception so that the Trip fails right away
        // instead of waiting for a WebContents until it times out.
        if (!(mHostStation instanceof WebContentsElementProvider provider)) {
            throw new AssertionError(
                    String.format(
                            "%s needs a host Station that implements WebContentsElementProvider,"
                                    + " not %s",
                            getName(), mHostStation.getName()));
        }
        return provider.getWebContentsElement().get();
    }
}
