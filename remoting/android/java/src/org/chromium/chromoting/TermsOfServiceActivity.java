// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

/**
 * The Activity for showing the terms of service.
 */
public class TermsOfServiceActivity extends WebViewActivity {
    private static final String TOS_URL = "https://policies.google.com/terms";

    public TermsOfServiceActivity() {
        super(R.string.terms_of_service, TOS_URL);
    }
}
