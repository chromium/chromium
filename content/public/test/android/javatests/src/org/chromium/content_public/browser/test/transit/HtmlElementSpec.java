// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.transit;

import org.chromium.build.annotations.NullMarked;

/** Specification of an HTML DOM element to produce an {@link HtmlElement}. */
@NullMarked
public class HtmlElementSpec {
    private final String mId;
    private final String mSelectorJs;
    private final String mDescription;

    /**
     * @param cssSelector CSS selector matching the DOM element, e.g. {@code "#id"}. The first
     *     matching element is used. It doesn't match inside shadow roots; use {@link
     *     #shadow(String)} for that.
     */
    public HtmlElementSpec(String cssSelector) {
        this(
                "HTML/" + cssSelector,
                "document.querySelector(" + quoteJsString(cssSelector) + ")",
                "selector=\"" + cssSelector + "\"");
    }

    private HtmlElementSpec(String id, String selectorJs, String description) {
        mId = id;
        mSelectorJs = selectorJs;
        mDescription = description;
    }

    /**
     * Returns a new {@link HtmlElementSpec} matching the first element that matches {@code
     * cssSelector} inside the open {@code shadowRoot} of this spec's element. Chain calls for
     * nested shadow roots. Matches nothing if this spec's element is missing or has no open shadow
     * root.
     */
    public HtmlElementSpec shadow(String cssSelector) {
        String chainedSelectorJs =
                "(function() { var h = "
                        + mSelectorJs
                        + "; return h && h.shadowRoot ? h.shadowRoot.querySelector("
                        + quoteJsString(cssSelector)
                        + ") : null; })()";
        return new HtmlElementSpec(
                mId + " >> " + cssSelector,
                chainedSelectorJs,
                mDescription + " >> selector=\"" + cssSelector + "\"");
    }

    public String getId() {
        return mId;
    }

    /** Returns a JS expression that evaluates to the DOM node or null. */
    public String getSelectorJs() {
        return mSelectorJs;
    }

    /** Returns a short human-readable description of the selector. */
    public String getDescription() {
        return mDescription;
    }

    static String quoteJsString(String s) {
        return "'" + s.replace("\\", "\\\\").replace("'", "\\'") + "'";
    }
}
