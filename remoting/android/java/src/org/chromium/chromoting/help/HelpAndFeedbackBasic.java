// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.help;

import android.app.Activity;

/**
 * This class implements a basic UI for help-and-feedback.
 */
public class HelpAndFeedbackBasic implements HelpAndFeedback {
    @Override
    public void launchHelp(Activity activity, @HelpContext int helpContext) {
        HelpActivity.launch(activity, urlFromHelpContext(helpContext));
    }

    @Override
    public void launchFeedback(Activity activity) {
        FeedbackSender.sendFeedback(activity);
    }

    private static String urlFromHelpContext(@HelpContext int helpContext) {
        switch (helpContext) {
            case HelpContext.HOST_LIST:
                return "https://support.google.com/chrome/answer/6002441#hosts";
            case HelpContext.HOST_SETUP:
                return "https://support.google.com/chrome/answer/1649523";
            case HelpContext.DESKTOP:
                return "https://support.google.com/chrome/answer/6002441#gestures";
            default:
                // Unreachable, but required by Java style.
                assert false;
                return "";
        }
    }
}
