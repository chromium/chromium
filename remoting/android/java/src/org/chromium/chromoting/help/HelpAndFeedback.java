// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.help;

import android.app.Activity;

/** Common interface for help-and-feedback implementations. */
public interface HelpAndFeedback {
    /**
     * Launches a new activity for displaying a Help screen. The implementation will show
     * information depending on the context, and will allow the user to submit a Feedback
     * report. The implementation may also grab a screenshot, so the caller should take
     * any steps to prepare the display before calling this method (for example, closing
     * navigation drawers).
     * @param activity Parent activity of the Help screen.
     * @param helpContext String used by the implementation to show context-based help.
     */
    void launchHelp(Activity activity, @HelpContext int helpContext);

    /**
     * Launches a new activity for displaying the feedback screen. The implementation may grab
     * a screenshot.
     * @param activity Parent activity of the feedback screen.
     */
    void launchFeedback(Activity activity);
}
