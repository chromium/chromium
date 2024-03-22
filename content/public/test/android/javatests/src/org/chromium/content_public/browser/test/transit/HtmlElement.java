// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.transit;

/** A Public Transit Element representing an HTML DOM element. */
public class HtmlElement {

    private final String mHtmlId;
    private final String mId;

    public HtmlElement(String id) {
        mHtmlId = id;
        mId = "HTML/#" + id;
    }

    public String getHtmlId() {
        return mHtmlId;
    }

    public String getId() {
        return mId;
    }
}
