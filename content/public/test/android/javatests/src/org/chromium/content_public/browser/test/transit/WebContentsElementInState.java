// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.transit;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.ElementInState;
import org.chromium.base.test.transit.InstrumentationThreadCondition;
import org.chromium.content_public.browser.WebContents;

import java.util.Set;

/** An available WebContents as an ElementInState to check whether HtmlElements are displayed. */
public class WebContentsElementInState implements ElementInState {

    private Supplier<WebContents> mWebContentsSupplier;

    public WebContentsElementInState(Supplier<WebContents> webContentsSupplier) {
        mWebContentsSupplier = webContentsSupplier;
    }

    @Override
    public String getId() {
        return "WebContents";
    }

    @Nullable
    @Override
    public Condition getEnterCondition() {
        return new InstrumentationThreadCondition() {
            @Override
            public ConditionStatus check() {
                return whether(mWebContentsSupplier.get() != null);
            }

            @Override
            public String buildDescription() {
                return "WebContents exist";
            }
        };
    }

    @Nullable
    @Override
    public Condition getExitCondition(Set<String> destinationElementIds) {
        return null;
    }

    public WebContents getWebContents() {
        return mWebContentsSupplier.get();
    }
}
