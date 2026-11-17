// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.transit;

import org.chromium.build.annotations.NullMarked;

/** Specification of an HTML DOM element to produce an {@link HtmlElement}. */
@NullMarked
public class HtmlElementSpec {
    private final String mCssSelector;
    private final String mId;

    /**
     * @param cssSelector CSS selector matching the DOM element, e.g. {@code "#id"}. The first
     *     matching element is used.
     */
    public HtmlElementSpec(String cssSelector) {
        mCssSelector = cssSelector;
        mId = "HTML/" + cssSelector;
    }

    public String getId() {
        return mId;
    }

    /** Returns a JS expression that evaluates to the DOM node or null. */
    public String getSelectorJs() {
        return "document.querySelector(" + quoteJsString(mCssSelector) + ")";
    }

    /** Returns a short human-readable description of the selector. */
    public String getDescription() {
        return "selector=\"" + mCssSelector + "\"";
    }

    static String quoteJsString(String s) {
        return "'" + s.replace("\\", "\\\\").replace("'", "\\'") + "'";
    }
}
