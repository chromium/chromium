// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.transit;

import org.chromium.base.test.transit.Element;
import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;

/** A Station that shows a page in a {@link WebContents}, e.g. to host a {@link WebPageFacility}. */
@NullMarked
public interface WebContentsElementProvider {
    /** Returns the Element that supplies the page's {@link WebContents}. */
    Element<WebContents> getWebContentsElement();
}
