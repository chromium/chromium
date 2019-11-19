// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.help;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Enumeration of contexts from which the user could request help in the application. The
 * HelpAndFeedback implementation is responsible for displaying an appropriate Help article for
 * each context.
 */
@IntDef({HelpContext.HOST_LIST, HelpContext.HOST_SETUP, HelpContext.DESKTOP})
@Retention(RetentionPolicy.SOURCE)
public @interface HelpContext {
    // Help for the host-list screen.
    int HOST_LIST = 0;

    // Help on setting up a new host.
    int HOST_SETUP = 1;

    // Help for the connected desktop screen, including touch gestures.
    int DESKTOP = 2;
}
